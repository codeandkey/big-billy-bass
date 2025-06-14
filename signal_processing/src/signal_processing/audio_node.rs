use libpulse_binding::{
    callbacks::ListResult,
    context::{
        Context, FlagSet as ContextFlagSet, State as ContextState,
        introspect::SourceInfo,
        subscribe::{self, Facility, InterestMaskSet},
    },
    mainloop::standard::{IterateResult, Mainloop},
    operation::{self, State as OperationState},
    proplist::{self},
    sample::{Format, Spec},
    stream::{self, FlagSet as StreamFlagSet, PeekResult, State as StreamState, Stream},
};
use regex::Regex;
use std::{cell::RefCell, rc::Rc};

const DEFAULT_SPEC: Spec = Spec {
    channels: 2,
    format: Format::S16NE,
    rate: 44100,
};

const SOURCE_FILTER: &str = "bluez_source\\..*\\.a2dp_source";
pub struct PaNode {
    _ml: Mainloop,
    _context: Context,
    _source_stream: Stream,
    _sink_stream: Stream,

    _active_stream_ndx: Rc<RefCell<i32>>,
}

pub enum ReadResult<'a> {
    Data(&'a [u8]),
    NotReady,
}

impl PaNode {
    pub fn new(app_name: &str) -> Result<Self, &'static str> {
        let mut props = proplist::Proplist::new().unwrap();
        props
            .set_str(proplist::properties::APPLICATION_NAME, app_name)
            .unwrap();

        // init main loop & context
        let mut ml = Mainloop::new().unwrap();
        let mut ctx = Context::new_with_proplist(&ml, app_name, &props).unwrap();

        ctx.connect(None, ContextFlagSet::NOAUTOSPAWN, None)
            .or_else(|_| return Err("Failed to connect Context"))?;

        Self::iterate_ml_until(&mut ml, || Self::context_is_ready(&ctx)).unwrap();
        debug!("Connected to Pulse Audio Context");

        // used to keep track of the active stream
        let strm_ndx = Rc::new(RefCell::new(-1));
        let strm_ndx_cpy = Rc::clone(&strm_ndx);

        // now subscribe to events in context changes
        ctx.set_subscribe_callback(Some(Box::new(move |facility, operation, index| {
            if let (Some(Facility::Source), Some(subscribe::Operation::Removed)) =
                (facility, operation)
            {
                if *strm_ndx_cpy.borrow() == index as i32 {
                    info!("Stream {} has disconnected", index);
                    *strm_ndx_cpy.borrow_mut() = -1;
                }
            }
        })));

        ctx.subscribe(InterestMaskSet::SOURCE, |success| {
            if success {
                debug!("Subscribed to source events successfully");
            } else {
                debug!("Failed to subscribe to source events");
            }
        });

        // dummy stream
        let strm = Stream::new(&mut ctx, "received_audio", &DEFAULT_SPEC, None)
            .expect("Failed to initialize stream");

        // playback stream
        let mut playback_stream = Stream::new(&mut ctx, "output_audio", &DEFAULT_SPEC, None)
            .expect("Failed to create output stream");

        // connect playback stream
        playback_stream
            .connect_playback(
                None, // Default sink
                None,
                stream::FlagSet::START_CORKED,
                None,
                None,
            )
            .expect("Failed to connect playback stream");

        return Ok(Self {
            _ml: ml,
            _context: ctx,
            _source_stream: strm,
            _sink_stream: playback_stream,

            _active_stream_ndx: strm_ndx,
        });
    }

    pub fn read(&mut self) -> Result<ReadResult, &'static str> {
        // first check if stream is in a readable state
        if *self._active_stream_ndx.borrow() < 0 {
            self.connect_to_bluez_stream()?;
            if *self._active_stream_ndx.borrow() < 0 {
                return Ok(ReadResult::NotReady);
            }
        }
        if !Self::iterate_ml_and_check(&mut self._ml, || {
            Self::stream_is_ready(&self._source_stream)
        })? {
            return Ok(ReadResult::NotReady);
        };
        // read from stream
        return match self
            ._source_stream
            .peek()
            .or_else(|_| return Err("Can't peek audio!"))?
        {
            PeekResult::Empty => Ok(ReadResult::NotReady),
            PeekResult::Hole(_) => {
                debug!("Hole in audio detected");
                self._source_stream
                    .discard()
                    .or_else(|_| Err("Cannot discard stream data!"))?;
                Ok(ReadResult::NotReady)
            }
            PeekResult::Data(data) => Ok(ReadResult::Data(data)),
        };
    }

    pub fn drop(&mut self) -> Result<(), &'static str> {
        // first check if stream is in a readable state
        if *self._active_stream_ndx.borrow() < 0 {
            self.connect_to_bluez_stream()?;
            if *self._active_stream_ndx.borrow() < 0 {
                return Ok(());
            }
        }
        if !Self::iterate_ml_and_check(&mut self._ml, || {
            Self::stream_is_ready(&self._source_stream)
        })? {
            return Ok(());
        };

        return self
            ._source_stream
            .discard()
            .or_else(|_| return Err("Can't peek audio!"));
    }

    pub fn playback(&mut self, chunk: &[u8]) -> Result<(), &'static str> {
        self._sink_stream
            .write(chunk, None, 0, stream::SeekMode::Relative)
            .expect("playback failed");
        Ok(())
    }

    fn connect_to_bluez_stream(&mut self) -> Result<(), &'static str> {
        let strm_ndx_cpy = Rc::clone(&self._active_stream_ndx);
        // set up source info list callback
        let op = self
            ._context
            .introspect()
            .get_source_info_list(move |list| {
                let match_reg = Regex::new(SOURCE_FILTER).unwrap();
                if let ListResult::Item(source) = list {
                    if let Some(name) = source.name.as_ref() {
                        if match_reg.is_match(name) {
                            info!("Detected source {}", name);
                            *strm_ndx_cpy.borrow_mut() = source.index as i32;
                        }
                    }
                }
            });

        Self::iterate_ml_until(&mut self._ml, || Self::operation_is_done(&op))?;
        if *self._active_stream_ndx.borrow() < 0 {
            return Ok(());
        }

        // create and connect new stream
        self._source_stream =
            Stream::new(&mut self._context, "audio stream", &DEFAULT_SPEC, None).unwrap();

        self._source_stream
            .connect_record(
                Some(&format!("{}", *self._active_stream_ndx.borrow())),
                None,
                StreamFlagSet::NOFLAGS,
            )
            .or_else(|_| {
                return Err(format!(
                    "Cannot connect to stream index {}",
                    *self._active_stream_ndx.borrow()
                ));
            })
            .unwrap();

        info!(
            "Connected to stream index {}",
            self._active_stream_ndx.borrow()
        );
        info!("{:?}", self._source_stream.get_sample_spec());
        Ok(())
    }

    fn operation_is_done(
        op: &operation::Operation<dyn FnMut(ListResult<&SourceInfo<'_>>)>,
    ) -> Result<bool, &'static str> {
        return match op.get_state() {
            OperationState::Running => Ok(false),
            _ => Ok(true),
        };
    }

    fn context_is_ready(ctx: &Context) -> Result<bool, &'static str> {
        match ctx.get_state() {
            ContextState::Ready => Ok(true),
            ContextState::Failed | ContextState::Terminated => {
                Err("Context state failed/terminated")
            }
            _ => Ok(false),
        }
    }

    fn stream_is_ready(strm: &Stream) -> Result<bool, &'static str> {
        match strm.get_state() {
            StreamState::Failed | StreamState::Terminated => {
                Err("stream state failed / terminated!")
            }
            StreamState::Ready => Ok(true),
            _ => Ok(false),
        }
    }

    fn iterate_ml_until<F>(ml: &mut Mainloop, mut operation: F) -> Result<(), &'static str>
    where
        F: FnMut() -> Result<bool, &'static str>,
    {
        while !Self::iterate_ml_and_check(ml, &mut operation)? {}
        Ok(())
    }

    fn iterate_ml_and_check<F>(ml: &mut Mainloop, mut operation: F) -> Result<bool, &'static str>
    where
        F: FnMut() -> Result<bool, &'static str>,
    {
        match ml.iterate(false) {
            IterateResult::Quit(_) | IterateResult::Err(_) => {
                return Err("Iterate state was not success");
            }
            IterateResult::Success(_) => {}
        }
        operation()
    }
}

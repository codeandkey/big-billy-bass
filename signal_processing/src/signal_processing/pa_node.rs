use common::param::{ParameterController, *};
use libpulse_binding::{
    callbacks::ListResult,
    context::{
        Context, FlagSet as ContextFlagSet, State as ContextState,
        subscribe::{Facility, InterestMaskSet, Operation as Opcode},
    },
    def::BufferAttr,
    mainloop::standard::Mainloop,
    sample::{Format, Spec},
    stream::{FlagSet as StreamFlagSet, PeekResult, SeekMode, Stream},
};
use regex::Regex;
use std::{cell::RefCell, error::Error, rc::Rc};

const DEFAULT_SPEC: Spec = Spec {
    channels: 2,
    format: Format::S16NE,
    rate: 44100,
};

enum StreamType {
    Source,
}

const SOURCE_FILTER: &str = "bluez_source\\..*\\.a2dp_source";
// fudge factor for equivilence when setting the buffer attribute of a stream in pulse audio
const LATENCY_FUDGE_FACTOR: u32 = 5;

/// Struct containing pulse audio data structures. Keeps track of the context, mainloop, and active sink/source streams.
pub struct PaNode {
    ml: Rc<RefCell<Mainloop>>,
    ctx: Rc<RefCell<Context>>,

    sink: Rc<RefCell<Option<Stream>>>,
    source: Rc<RefCell<Option<Stream>>>,
}

impl PaNode {
    /// Creates a new Pulse audio "node" instance. The node is set up to
    /// actively search for bluez sources devices and set up source-output link
    /// from said sources. A playback stream with a default device is created so the PA server can
    /// determine which output devices to send the stream to.
    ///
    /// # Returns
    /// A new `PaNode` instance wrapped in a result.
    pub fn new() -> Result<Self, Box<dyn Error>> {
        let ml = Rc::new(RefCell::new(
            Mainloop::new().ok_or("Unable to create mainloop")?,
        ));
        let ctx = Rc::new(RefCell::new(
            Context::new(&*ml.borrow_mut(), "BBB-pulseaudio")
                .ok_or("Unable to initialize PA context")?,
        ));

        ctx.borrow_mut()
            .connect(None, ContextFlagSet::NOAUTOSPAWN, None)?;

        let ctx_clone = Rc::clone(&ctx);
        ctx.borrow_mut().set_state_callback(Some(Box::new(move || {
            match ctx_clone.borrow().get_state() {
                ContextState::Ready => info!("Context Initailized!"),
                ContextState::Failed | ContextState::Terminated => {
                    warn!("Context failed or terminated")
                }
                _ => {}
            }
        })));

        info!("Waiting for context to connect");
        while ctx.borrow().get_state() != ContextState::Ready {
            ml.borrow_mut().iterate(true);
        }

        let sink = Rc::new(RefCell::new(Stream::new(
            &mut *ctx.borrow_mut(),
            "bbb-output",
            &DEFAULT_SPEC,
            None,
        )));

        if let Some(sink) = sink.borrow_mut().as_mut() {
            let _ = sink.connect_playback(None, None, StreamFlagSet::NOFLAGS, None, None);
        }

        Ok(Self {
            ml,
            ctx,
            source: Rc::new(RefCell::new(None)),
            sink,
        })
    }

    /// Creates a new Pulse audio "node" instance. The node is set up to
    /// actively search for bluez sources devices and set up source-output link
    /// from said sources. A playback stream with a default device is created so the PA server can
    /// determine which output devices to send the stream to.
    ///
    /// # Returns
    /// A new `PaNode` reference, wrapped in a result
    pub fn new_ref() -> Result<Rc<RefCell<Self>>, Box<dyn Error>> {
        Ok(Rc::new(RefCell::new(Self::new()?)))
    }

    /// Creates a stream from a bluez source if immediately available, otherwise an event based callback will be used.
    ///
    ///
    /// The event callback subscribes to pulse audio events, specifically `New` or `Removed` `Source` events (see the pulse audio docs).
    /// A new stream is generated when a new bluez source is detected.
    ///
    /// Once a stream is created, the read callback is set. The user callback is wrapped in an internal callback.
    /// The order of events are:
    /// 1) The stream is read from, and the bytes are converted to PCM_S16 data
    /// 2) PCM data is passed to the user callback
    /// 3) the PCM data is passed to the sink stream.
    /// 
    /// The sink stream is configured for additional latency based on the RMS window size in side the read callback.
    /// 
    /// The user callback should take a `Vec<i16>`. This may be interpreted as PCM16 interleaved two-channel data.
    /// 
    /// # Arguments
    /// * `node` - Reference to a `Rc<RefCell<PaNode>>`
    /// * `cb` - user callback wrapped in an `Rc<RefCell<FnMut(Vec<i16>) + static>>`
    ///
    /// # Returns
    /// Returns an empty result on success
    ///
    pub fn set_read_callback<F>(
        node: &Rc<RefCell<Self>>,
        cb: Rc<RefCell<F>>,
    ) -> Result<(), Box<dyn Error>>
    where
        F: FnMut(Vec<i16>) + 'static,
    {
        // first do a quick check of available sources to connect to
        Self::refresh_sources(
            Rc::clone(&node.borrow().ctx),
            Rc::clone(&node.borrow().source),
            Rc::clone(&node.borrow().sink),
            Rc::clone(&cb),
        )?;

        // then set up the callbacks for pulse audio
        let source_ref = Rc::clone(&node.borrow().source);
        let sink_ref = Rc::clone(&node.borrow().sink);
        node.borrow()
            .ctx
            .borrow_mut()
            .set_subscribe_callback(Some(Box::new({
                let context = Rc::clone(&node.borrow().ctx);
                move |event, op, _| {
                    if let (Some(event), Some(op)) = (event, op) {
                        match op {
                            Opcode::New | Opcode::Removed => match event {
                                Facility::Source => {
                                    if op == Opcode::Removed {
                                        info!("Source removed");
                                        return;
                                    }
                                    Self::refresh_sources(
                                        Rc::clone(&context),
                                        Rc::clone(&source_ref),
                                        Rc::clone(&sink_ref),
                                        Rc::clone(&cb),
                                    )
                                    .unwrap();
                                }
                                _ => {}
                            },
                            _ => {}
                        }
                    }
                }
            })));

        node.borrow()
            .ctx
            .borrow_mut()
            .subscribe(InterestMaskSet::SINK | InterestMaskSet::SOURCE, |_| {});

        Ok(())
    }

    /// Runs the pulse audio mainloop. Blocks until the loop exits.
    pub fn run_mainloop(&mut self) -> Result<(), Box<dyn Error>> {
        let res = self.ml.borrow_mut().run();
        match res {
            Ok(_) => Ok(()),
            Err((code, _)) => Err(format!(
                "Pa returned error code {} after exiting the mainloop",
                code
            ))?,
        }
    }

    /// Private helper which refreshes the available source list
    /// from the existing context object. The streams (source) is updated if a
    /// new device is connected to. The user read callback is passed through here
    /// to set up the read callback of any new stream.
    fn refresh_sources<F>(
        ctx: Rc<RefCell<Context>>,
        stream: Rc<RefCell<Option<Stream>>>,
        sink: Rc<RefCell<Option<Stream>>>,
        cb: Rc<RefCell<F>>,
    ) -> Result<(), Box<dyn Error>>
    where
        F: FnMut(Vec<i16>) + 'static,
    {
        let source_pattern = Regex::new(SOURCE_FILTER).unwrap();
        // Fetch sources
        let ctx_clone = Rc::clone(&ctx);
        ctx.borrow().introspect().get_source_info_list({
            let source_pattern = source_pattern.clone();
            move |info| {
                if let ListResult::Item(info) = info {
                    let source_name = info.name.as_ref().unwrap();
                    if !source_pattern.is_match(source_name) {
                        return;
                    }
                    info!(
                        "Detected new source: [{}] {}",
                        info.description.as_ref().unwrap(),
                        source_name
                    );
                    *stream.borrow_mut() = Self::create_and_connect_stream(
                        Some(source_name.to_string()),
                        StreamType::Source,
                        &mut *ctx_clone.borrow_mut(),
                    );

                    Self::_set_stream_cb(Rc::clone(&stream), Rc::clone(&sink), Rc::clone(&cb))
                        .unwrap();
                }
            }
        });
        Ok(())
    }

    /// Private helper to streamline creating a pulse audio stream
    fn create_and_connect_stream(
        pa_stream_name: Option<String>,
        stream_type: StreamType,
        ctx: &mut Context,
    ) -> Option<Stream> {
        if let Some(name) = pa_stream_name.as_ref().map(|s| &**s) {
            match stream_type {
                StreamType::Source => {
                    let mut stream = Stream::new(ctx, "bbb-capture", &DEFAULT_SPEC, None)?;
                    debug!("Attempting to connect to source: {}", name);
                    stream
                        .connect_record(Some(name), None, StreamFlagSet::NOFLAGS)
                        .expect("Unable to connect to stream");
                    return Some(stream);
                }
            };
        }
        None
    }

    /// Sets the read callback for a given stream.
    /// The order of events are:
    /// 1) The stream is read from, and the bytes are converted to PCM_S16 data
    /// 2) PCM data is passed to the user callback
    /// 3) the PCM data is passed to the sink stream.
    ///
    /// Due to latency/phase shift induced from the moving RMS window in the main, the
    /// sink buffer size is adjusted to account for a phase shift as well.
    fn _set_stream_cb<F>(
        source_ref: Rc<RefCell<Option<Stream>>>,
        sink_ref: Rc<RefCell<Option<Stream>>>,
        user_cb: Rc<RefCell<F>>,
    ) -> Result<(), Box<dyn Error>>
    where
        F: FnMut(Vec<i16>) + 'static,
    {
        if let Some(source) = source_ref.borrow_mut().as_mut() {
            let read_ref = Rc::clone(&source_ref);
            let pc = ParameterController::new()?;
            let mut delay_stored: bool = false;
            let mut delay_default: u32 = 0;

            source.set_read_callback(Some(Box::new(move |_| {
                let mut binding = read_ref.borrow_mut();
                let stream = binding.as_mut().unwrap();
                match stream.peek().unwrap() {
                    // convert bytes into PCM 16 data per the default spec
                    PeekResult::Data(bytes) => {
                        let mut output = Vec::<i16>::with_capacity(bytes.len() / 2);
                        for chunk in bytes.chunks(2) {
                            output.push(i16::from_le_bytes([chunk[0], chunk[1]]));
                        }
                        user_cb.borrow_mut()(output);

                        if let Some(sink) = sink_ref.borrow_mut().as_mut() {
                            let attr = sink.get_buffer_attr().unwrap();
                            let rms_window_ms = pc.get::<f32>(PARAM_RMS_WINDOW_SIZE_MS);
                            let window_size = (44100.0 * rms_window_ms / 1000.0) as u32;
                            if !delay_stored {
                                delay_default = attr.tlength;
                                delay_stored = true;
                            }

                            // tweak latency if it has moved outside of our target
                            if attr.tlength.abs_diff(delay_default + window_size * 3 / 4)
                                > LATENCY_FUDGE_FACTOR
                            {
                                debug!(
                                    "Adjusting latency to {} samples (from {})",
                                    (delay_default + window_size * 3 / 4),
                                    attr.tlength
                                );
                                let new_attr = BufferAttr {
                                    maxlength: attr.maxlength,
                                    minreq: attr.minreq,
                                    prebuf: attr.prebuf,
                                    fragsize: attr.fragsize,

                                    tlength: delay_default + window_size * 3 / 4,
                                };

                                sink.set_buffer_attr(&new_attr, |_| {});
                            }

                            _ = sink.write(bytes, None, 0, SeekMode::Relative);
                        }
                        stream.discard().unwrap();
                    }
                    PeekResult::Hole(_) => {
                        stream.discard().unwrap();
                    }
                    PeekResult::Empty => {}
                }
            })));
        }
        Ok(())
    }
}

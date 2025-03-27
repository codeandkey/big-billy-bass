use common::param::*;
use ratatui::{
    Frame,
    layout::Rect,
    widgets::{Block, Borders, List, ListDirection},
};

pub fn render(frame: &mut Frame, rect: Rect, pc: &mut ParameterController) {
    let block = Block::new().title("PARAMS").borders(Borders::ALL);

    let paramlist = ALL_PARAMS
        .iter()
        .map(|Parameter(pn, _pv, _f1, _f2, _f3)| *pn);
    let maxlen = paramlist.map(str::len).reduce(usize::max).unwrap();

    let listvals = ALL_PARAMS.iter().map(|Parameter(pn, _pv, _f1, _f2, _f3)| {
        let pv = pc.get::<String>(&Parameter(pn, _pv, *_f1, *_f2, *_f3));
        format!("{:width$} {}", pn, pv, width = maxlen)
    });

    let list = List::new(listvals).direction(ListDirection::TopToBottom);

    frame.render_widget(list, rect);
}

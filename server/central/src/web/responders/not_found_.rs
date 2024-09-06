use std::borrow::Cow;

use super::*;

#[derive(Responder)]
#[response(status = 404, content_type = "text")]
pub struct NotFoundResponder {
    pub inner: String,
}

impl NotFoundResponder {
    pub fn new<T: Into<Cow<'static, str>>>(inner: T) -> Self {
        let inner = match inner.into() {
            Cow::Borrowed(borrowed) => borrowed.to_owned(),
            Cow::Owned(owned) => owned,
        };

        Self { inner }
    }
}

impl From<NotFoundResponder> for GenericErrorResponder<String> {
    fn from(value: NotFoundResponder) -> Self {
        GenericErrorResponder {
            inner: (Status::NotFound, value.inner),
        }
    }
}
macro_rules! not_found {
    ($msg:expr) => {
        return Err(NotFoundResponder::new($msg).into())
    };
}

pub(crate) use not_found;

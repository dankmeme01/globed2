#![feature(sync_unsafe_cell, duration_constructors, async_closure, let_chains)]
#![allow(
    clippy::must_use_candidate,
    clippy::module_name_repetitions,
    clippy::cast_possible_truncation,
    clippy::missing_errors_doc,
    clippy::missing_panics_doc,
    clippy::missing_safety_doc,
    clippy::wildcard_imports,
    clippy::redundant_closure_for_method_calls
)]

pub mod bridge;
pub mod client;
pub mod data;
pub mod managers;
pub mod server;
pub mod state;
pub mod util;
use globed_shared::webhook;

#[cfg(feature = "use_tokio_tracing")]
use tokio_tracing as tokio;

#[cfg(not(feature = "use_tokio_tracing"))]
#[allow(clippy::single_component_path_imports)]
use tokio;

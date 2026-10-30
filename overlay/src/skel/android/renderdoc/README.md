# RenderDoc application API header

`renderdoc_app.h` is the unmodified MIT-licensed application API header shipped
with the locally installed RenderDoc Meta Fork. Copyright and license are
retained in the file. Upstream source/documentation:
https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/api/app/renderdoc_app.h
https://github.com/baldurk/renderdoc/blob/v1.x/docs/in_application_api.rst

The integration requests the backward-compatible 1.4.0 API prefix. No RenderDoc
binary is linked, bundled or loaded by the game. Runtime discovery is passive
and capture must be explicitly enabled for a replay launched under RenderDoc.

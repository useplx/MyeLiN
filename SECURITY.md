# Security policy

## Supported versions

Security fixes are applied to the latest v1 release. Breaking security changes
that cannot preserve the v1 ABI are released under a new major version.

## Artifact trust boundary

The Runtime validates the `.mln` container structure before consuming its
sections. This protects the parser from malformed offsets, ranges, duplicate
records, and unsupported format versions. It does not make an artifact a
sandboxed document.

An `.mln` may contain Native providers and drivers. Loading those sections is
equivalent to loading executable code. Wasm orchestration has a narrower host
interface, but the complete artifact must still be treated as executable.

Production deployments must therefore authenticate artifacts before passing
them to `Session::load` or `mln_session_load`. The version 1 container does not
embed a cryptographic signature. Signature verification belongs in the
deployment/update chain until a future container version defines an
interoperable signed representation.

Do not load artifacts received directly from an untrusted peer. Do not run the
Runtime with privileges that the model application does not need.

## Checkpoints and packages

SYNaPSe packages execute host build plugins and may compile Native providers.
Only resolve packages and checkpoints from trusted, pinned sources. Review
lockfile changes before building release artifacts.

## Reporting

Report suspected vulnerabilities privately to the project maintainers. Include
the affected revision, platform, reproduction steps, and whether the issue is
reachable while parsing a malformed container or requires executing a trusted
package.

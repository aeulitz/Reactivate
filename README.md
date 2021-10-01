
# General
Project to try out on-demand detouring of activation methods for React Native.

# Dev Notes
## Setup

### Detours
- Enlist in Detours.
  - AFAIK, Detours does not publish artefacts, you need to enlist in it to get its header and its link lib.
- Build Detours for the same platform and configuration you want to build this project for.
- In this project ...
  - Add "\<DetoursDir\>\include" to "[Project Properties]/Configuration Properties/VC++ Directories/Include Directories".
  - Add "\<DetoursDir\>\lib.X64" to "[Project Properties]/Configuration Properties/Linker/General/Additional Library Directories".
  - Add "detours.lib" to "[Project Properties]/Configuration Properties/Linker/Input/Additional Dependencies".

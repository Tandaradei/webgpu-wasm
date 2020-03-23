# GET IT READY
* Install latest emsdk https://emscripten.org/docs/getting_started/downloads.html
* Clone this repository `git clone https://github.com/Tandaradei/webgpu-wasm`
* `cd webgpu-wasm`
* `git submodule init`
* `git submodule update`

# BUILD
*Currently the output files are also in the repository, so you don't have to build it by yourself*
* cd into the base directory of *webgpu-wasm*
* Needs to be done once at the start of a session
    * `pushd <PATH_TO_EMSDK>`
    * `source emsdk_env.sh`
    * `popd`
* If first time building or shader code was updated
    * `./compile_shaders.sh`
* Compile the program
    * `./compile.sh`

# RUN
* cd into the base directory of *webgpu-wasm*
* *[Once]* Start a basic web server
    * for example with python `python3 -m http.server 8080`
* Go to **localhost:8080** with a webgpu compatible browser
    * https://github.com/gpuweb/gpuweb/wiki/Implementation-Status
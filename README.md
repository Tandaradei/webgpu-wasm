# STATE OF WORK (2020-03-27)
![Test scene](demo/state_of_work_20200327.png)

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
    * `source setup_emscripten` (it assumes that the *emsdk* folder is in the same directory as the *webgpu-wasm* folder)
* Either build everything with
    * `./build.sh`
* Or separately in this order
    * `./compile_shaders.sh`
    * `./compile.sh`

# RUN
* cd into the base directory of *webgpu-wasm*
* *[Once]* Start a basic web server
    * for example with `./start_webserver.sh`
* Go to **localhost:8080** with a webgpu compatible browser
    * https://github.com/gpuweb/gpuweb/wiki/Implementation-Status
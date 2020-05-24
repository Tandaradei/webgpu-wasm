# STATE OF WORK
## 2020-05-24
![All objects imported from glTF files](demo/state_of_work_20200524.png)
## 2020-05-07
![Avocado imported from glTF file](demo/state_of_work_20200507.png)
## 2020-04-08
![Test scene](demo/state_of_work_20200408.png)
## 2020-03-27
![Test scene](demo/state_of_work_20200327.png)



# PREPARE
* Install latest emsdk https://emscripten.org/docs/getting_started/downloads.html
* Clone this repository `git clone https://github.com/Tandaradei/webgpu-wasm`
* `cd webgpu-wasm`
* `git submodule init`
* `git submodule update`

# BUILD
## DEPENDENCIES
* emsdk
* CMake
* glslc
## PROCESS
* cd into the base directory of *webgpu-wasm*
* Needs to be done once at the start of a session
    * `source setup_emscripten` (it assumes that the *emsdk* folder is in the same directory as the *webgpu-wasm* folder)
* emcmake cmake -DCMAKE_BUILD_TYPE=<Release|Debug> build
* cmake --build build
    * The output artifact will be emitted in the *webgpu-wasm/out* directory

# RUN
* cd into the base directory of *webgpu-wasm*
* *[Once]* Start a basic web server in the *out* directory
    * for example with `./start_webserver.sh` (automatically starts a simple python web server in the *out* directory)
* Go to **localhost:8080** with a webgpu compatible browser
    * https://github.com/gpuweb/gpuweb/wiki/Implementation-Status
* Choose one of the presented builds
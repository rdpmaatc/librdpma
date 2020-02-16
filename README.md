# librdpma

Efficient Remote Persistent Write using Remote Direct Memory Access (RDMA) and Non-volatile Memory (NVM).

## Build 

Building benchmark executables is fairly easy, we use cmake script to automatically download and build dependent libraries.

- `git submodule update --init`
- `cmake .; make`;

## Run

For example, when running one-sided remote persistent write bench, and a server equipped with NVM is at a server named "r740".
At server, run `sudo ./nvm_server --port=6666 -use_nvm=true --nvm_sz=10 --nvm_file=path_to_nvm`.
Then at client, run `nvm_client -addr="r740:6666" --coros=8 --threads=20 --id=0 --use_nic_idx=1 --use_read=false --payload=64 --add_sync=true --address_space=2 --random=true`. 

For more information about how to configure the runtime behavior of running server and client by passing command line parameters,
use `./nvm_server --help` and `./nvm_client --help`.

Evaluation scripts are listed in `scripts`.

## Code structure

`benchs`: code of main evaluation benchmarks.

`ddio_tool`: tools for enabling/disabling, and detecting DDIO on Xeon processors. check `ddio_tool/README.md` for more information.

`third_party`: `third_party/r2` and `third_party/rlib` is a snapshot of the execution framework, and rdma lib used in DrTM+H

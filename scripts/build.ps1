Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build --config Release
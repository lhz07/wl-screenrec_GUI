# wl-screenrec_GUI
A simple GUI for wl-screenrec

## Build
### Dependencies
- cmake
- qt6
- wayland
	- wayland-protocols >=1.32
- cairo
- libxkbcommon
- wl-screenrec
#### Install Dependencies
##### Arch
```sh
pacman -S cmake qt6-base pkgconf libxkbcommon cairo wayland wayland-protocols
```
##### Ubuntu / Debian
```sh
apt install cmake qt6-base-dev pkg-config libxkbcommon-dev libcairo2-dev libwayland-dev wayland-protocols 
```
### Build and Run
```sh
git -c url."https://github.com/".insteadOf="git@github.com:" clone --recursive https://github.com/lhz07/wl-screenrec_GUI.git
cd wl-screenrec_GUI
mkdir build
cd build
cmake ..
cmake --build . --parallel
```
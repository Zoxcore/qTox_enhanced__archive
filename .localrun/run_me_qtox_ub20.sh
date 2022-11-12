#! /bin/bash


_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_

mkdir -p $_HOME_/toxid_save
mkdir -p $_HOME_/tox_data

systems__="qtox_push_002_ub20"

echo '#! /bin/bash
# go to interactive shell inside the container ---------
cd /workspace/
export GTK_USE_PORTAL=1
./qtox
' > $_HOME_/ubuntu_20.04/artefacts/init_and_bash.sh
chmod u+x $_HOME_/ubuntu_20.04/artefacts/init_and_bash.sh

cd $_HOME_/

# this allows "local" (non networked connections to your local display) !!!
xhost +local:root
# this allows "local" (non networked connections to your local display) !!!

docker run --rm=false \
  -v $_HOME_/ubuntu_20.04/artefacts:/workspace \
  --privileged \
  -v /dev/snd:/dev/snd \
  -v /dev/video0:/dev/video0 \
  -v /dev/shm:/dev/shm \
  -v /etc/machine-id:/etc/machine-id \
  -v /run/user/$uid/pulse:/run/user/$uid/pulse \
  -v /var/lib/dbus:/var/lib/dbus \
  -v ~/.pulse:/root/.pulse \
  -v $_HOME_/tox_data:/root/ \
  -v $_HOME_/toxid_save:/root/.config/tox \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v /run/dbus/:/run/dbus/:rw \
  --net=host \
  -v "$HOME/.Xauthority:/root/.Xauthority:rw" \
  -i -t \
  "$systems__" \
  bash /workspace/init_and_bash.sh

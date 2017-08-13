#!/bin/sh

set -e
set -x
eval "$(/bin/sh ../remote-config.sh)"

PROJECT=line-router
EXTRA_SOURCES='line-gui util tomo remote_config.h'
BUILD_DIR=$PWD
SRC_DIR=$SVN_DIR/$PROJECT
REMOTE_USER=$REMOTE_USER_ROUTER
REMOTE_HOST=$REMOTE_HOST_ROUTER
REMOTE_DIR=/root
MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 CONFIG+=$BUILD_CONFIG_ROUTER && make -w && make install"

cp $SRC_DIR/deploycore-template.pl $SRC_DIR/deploycore.pl
perl -pi -e "s/REMOTE_DEDICATED_IF_ROUTER/$REMOTE_DEDICATED_IF_ROUTER/g" $SRC_DIR/deploycore.pl
perl -pi -e "s/REMOTE_DEDICATED_IP_HOSTS/$REMOTE_DEDICATED_IP_HOSTS/g" $SRC_DIR/deploycore.pl

cd $BUILD_DIR || exit 1
rm -f $PROJECT.zip || exit 1
cd .. || exit 1
pwd; echo "Zipping $PROJECT..."
zip -r $BUILD_DIR/$PROJECT.zip $PROJECT $EXTRA_SOURCES || exit 1
cd $BUILD_DIR || exit 1
pwd; echo "Copying $PROJECT.zip to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
scp $PROJECT.zip $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
rm -f stdoutfifo
rm -f stderrfifo
mkfifo stdoutfifo || exit 1
cat stdoutfifo &
mkfifo stderrfifo || exit 1
(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PROJECT|$SRC_DIR|g;" >&2) &
pwd; echo "Building remotely..."
ssh $REMOTE_USER@$REMOTE_HOST "sh -c \"pwd && cd $REMOTE_DIR && pwd && rm -rf $PROJECT && unzip -o $PROJECT.zip && cd $PROJECT && find . -exec touch {} \; && $MAKE\"" 1>stdoutfifo 2>stderrfifo
rm stdoutfifo
rm stderrfifo

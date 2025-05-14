# Exit on error and print commands
set -e

echo "=== 시스템 업데이트 시작 ==="
sudo apt update && sudo apt full-upgrade -y

echo "=== 필수 패키지 설치 ==="
sudo apt install -y \
  bluetooth \
  bluez \
  bluez-tools \
  libbluetooth-dev \
  libusb-dev \
  libreadline-dev \
  libglib2.0-dev \
  libudev-dev \
  libdbus-1-dev \
  libical-dev \
  python3 \
  python3-pip \
  redis-server \
  autoconf \
  git

echo "=== Redis 서비스 설정 ==="
sudo systemctl start redis-server
sudo systemctl enable redis-server

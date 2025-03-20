set -o errexit

# Install required packages
apt-get update
apt-get install -y build-essential cmake
apt-get install -y libopenblas-dev liblapack-dev
apt-get install -y libx11-dev libgtk-3-dev
apt-get install -y python3-opencv

# Install dependencies
pip install -r requirements.txt

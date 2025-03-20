# sudo apt-get update
# $ sudo apt-get install build-essential cmake
# $ sudo apt-get install libopenblas-dev liblapack-dev
# $ sudo apt-get install libx11-dev libgtk-3-dev

set -o errexit

# Install required packages
sudo apt-get update
sudo apt-get install -y build-essential cmake
sudo apt-get install -y libopenblas-dev liblapack-dev
sudo apt-get install -y libx11-dev libgtk-3-dev
sudo apt-get install -y python3-opencv

# Install dependencies
pip install -r requirements.txt

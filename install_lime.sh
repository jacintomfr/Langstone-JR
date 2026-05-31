#!/bin/bash
# Langstone-V3 Install lime script 

# Install LimeSuite 22.09 as at 27 Feb 23
# Commit 9c983d872e75214403b7778122e68d920d583add
echo
echo "#######################################"
echo "##### Installing LimeSuite 22.09 #####"
echo "######################################"
wget https://github.com/myriadrf/LimeSuite/archive/9c983d872e75214403b7778122e68d920d583add.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-9c983d872e75214403b7778122e68d920d583add LimeSuite
rm -rf LimeSuite-9c983d872e75214403b7778122e68d920d583add
rm master.zip

# Compile LimeSuite
cd LimeSuite/
mkdir dirbuild
cd dirbuild/
cmake ../
make
sudo make install
sudo ldconfig
cd /home/pi

# Install udev rules for LimeSuite
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/pi/LimeSuite/udev-rules/install.sh
cd /home/pi	

# Record the LimeSuite Version	
echo "9c983d8" >/home/pi/LimeSuite/commit_tag.txt



echo "#################################"
echo "##        Install gr-limesdr   ##"
echo "#################################"

#git clone https://github.com/myriadrf/gr-limesdr.git
cd gr-limesdr
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig

cd ~

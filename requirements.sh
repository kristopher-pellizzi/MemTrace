#! /bin/sh

pkgs="ninja-build libglib2.0-dev make gcc g++ pkg-config"

print_requirements(){
	echo "Required packages (note that these are apt-based package names):"
	for pkg in $pkgs; do
		echo "[*] $pkg"
	done
}

dpkg_status=$(dpkg-query --help > /dev/null 2>&1)
if [ $? -ne 0 ]; then
	echo "dpkg-query not available. Probably your current OS is using a package manager different from apt"
	echo "Please, install the required packages manually, then launch make"
	echo ""
	print_requirements
	exit 0
fi

for pkg in $pkgs; do
	status=$(dpkg-query -W -f='${db:Status-Status}' "$pkg" 2>/dev/null | grep -c "installed") 
	echo ""
	if [ $? -ne 0 ] || [ $status -eq 0 ]; then
		echo "$pkg not installed"
		echo "Attempt to install the missing package. NOTE: SUDO PASSWORD MAY BE REQUESTED"
		sudo apt-get -y install $pkg
		if [ $? -eq 0  ]; then
			echo "$pkg successfully installed"
		else
			echo "$pkg installation failed. Please manually install the required packages and retry."
			print_requirements
			exit 1
		fi
		echo ""
		echo ""
	else
		echo "$pkg already installed"
	fi
done

# Maintainer: codereader <greebo[AT]angua[DOT]at>
pkgname=radianteditor
pkgver=3.8.0
pkgrel=1
pkgdesc="3D level editor for IdTech4 games"
arch=("x86_64")
url="https://github.com/FriskTheFallenHuman/RadiantEditor"
license=("GPL")
depends=(wxgtk2 ftgl glew freealut libvorbis python libsigc++ eigen)
makedepends=(cmake git)
source=("$pkgname::git+https://github.com/FriskTheFallenHuman/RadiantEditor.git#tag=3.8.0")
md5sums=("SKIP")

build() {
	cd "$pkgname"
	cmake .
	make
}

package() {
	cd "$pkgname"
	make DESTDIR="$pkgdir/" install
}

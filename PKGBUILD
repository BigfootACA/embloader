# Maintainer: BigfootACA <bigfoot@classfun.cn>

pkgname=embloader
_pkgver=0.1
pkgver=$_pkgver
pkgrel=1
pkgdesc="Embedded Bootloader"
arch=(x86_64 aarch64)
url="https://github.com/BigfootACA/embloader"
license=(GPL-2.0-or-later)
makedepends=(gcc git make nasm python)
options=(!debug !strip)
source=(build.sh)
md5sums=(SKIP)

pkgver(){
	cd "$(dirname "$(realpath "$srcdir/build.sh")")"
	ver="$(git describe --long --tags 2>/dev/null | sed 's/^[vV]//;s/\([^-]*-g\)/r\1/;s/-/./g')"
	if ! [ -z "$ver" ]; then
		echo "$ver"
		return
	fi
	cnt="$(git rev-list --count HEAD 2>/dev/null||true)"
	sha="$(git rev-parse --short HEAD 2>/dev/null||true)"
	if [ -n "$cnt" ] && [ -n "$sha" ]; then
		echo "${_pkgver}.r${cnt}.${sha}"
		return
	fi
	echo "${_pkgver}"
}

build(){
	cd "$(dirname "$(realpath "$srcdir/build.sh")")"
	find build/Build/embloader/ -name '*.efi' -delete 2>/dev/null || true
	find build/Build/embloader/ -name '*.debug' -delete 2>/dev/null || true
	bash build.sh
}

package() {
	cd "$(dirname "$(realpath "$srcdir/build.sh")")"
        install -Dm644 build/Build/embloader/*_*/*/embloader.efi \
                $pkgdir/usr/share/embloader/embloader.efi
        install -Dm644 build/Build/embloader/*_*/*/embloader.debug \
                $pkgdir/usr/share/embloader/embloader.debug
        install -d $pkgdir/usr/share/doc/embloader/
        cp -a docs/* $pkgdir/usr/share/doc/embloader/
        install -d $pkgdir/usr/share/embloader/configs/
        cp -a configs/* $pkgdir/usr/share/embloader/configs/
        if [ -f README.md ]; then install -m644 README.md $pkgdir/usr/share/doc/embloader/; fi
        if [ -f LICENSE ]; then install -m644 LICENSE $pkgdir/usr/share/doc/embloader/; fi
}

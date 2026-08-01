# Maintainer: BigfootACA <bigfoot@classfun.cn>
# shellcheck shell=bash disable=SC2034,SC2154

pkgname=embloader
pkgver=0.7
pkgrel=1
pkgdesc="Embedded Bootloader"
arch=(x86_64 aarch64)
url="https://github.com/BigfootACA/embloader"
license=(GPL-2.0-or-later)
makedepends=(gcc git make nasm python)
options=(!debug !strip)
source=(
	"$pkgname::git+$url.git"
	"newlib::git+https://sourceware.org/git/newlib-cygwin.git"
	"json-c::git+https://github.com/json-c/json-c"
	"dtc::git+https://git.kernel.org/pub/scm/utils/dtc/dtc.git"
	"libufdt::git+https://android.googlesource.com/platform/system/libufdt"
	"stb::git+https://github.com/nothings/stb"
	"libyaml::git+https://github.com/yaml/libyaml"
	"lvgl::git+https://github.com/BigfootACA/lvgl"
	"nanosvg::git+https://github.com/memononen/nanosvg"
	"edk2::git+https://github.com/tianocore/edk2"
	"brotli::git+https://github.com/google/brotli"
	"mipisyst::git+https://github.com/MIPI-Alliance/public-mipi-sys-t.git"
)
sha256sums=(
	SKIP SKIP SKIP SKIP SKIP SKIP
	SKIP SKIP SKIP SKIP SKIP SKIP
)

pkgver(){
	cd "$srcdir/$pkgname" || return 1
	bash scripts/version.sh
}

prepare() {
	cd "$srcdir/$pkgname" || return 1

	local entry path source_name
	local -a paths=()
	local -a submodules=(
		"embloader/ext/newlib:newlib"
		"embloader/ext/json-c:json-c"
		"embloader/ext/dtc:dtc"
		"embloader/ext/libufdt:libufdt"
		"embloader/ext/stb:stb"
		"embloader/ext/libyaml:libyaml"
		"embloader/ext/lvgl:lvgl"
		"embloader/ext/nanosvg:nanosvg"
		"edk2:edk2"
	)
	for entry in "${submodules[@]}"; do
		path=${entry%%:*}
		source_name=${entry#*:}
		paths+=("$path")
		git config "submodule.$path.url" "$srcdir/$source_name"
	done
	git -c protocol.file.allow=always submodule update --init -- "${paths[@]}"

	git -C edk2 config \
		"submodule.BaseTools/Source/C/BrotliCompress/brotli.url" \
		"$srcdir/brotli"
	git -C edk2 config \
		"submodule.MdePkg/Library/MipiSysTLib/mipisyst.url" \
		"$srcdir/mipisyst"
	git -C edk2 -c protocol.file.allow=always submodule update --init -- \
		BaseTools/Source/C/BrotliCompress/brotli \
		MdePkg/Library/MipiSysTLib/mipisyst
}

build(){
	cd "$srcdir/$pkgname" || return 1
	find build/Build/embloader/ -name '*.efi' -delete 2>/dev/null || true
	find build/Build/embloader/ -name '*.debug' -delete 2>/dev/null || true
	bash build.sh
}

package() {
	cd "$srcdir/$pkgname" || return 1
	install -Dm644 build/Build/embloader/*_*/*/embloader.efi \
		"$pkgdir/usr/share/embloader/embloader.efi"
	install -Dm644 build/Build/embloader/*_*/*/embloader.debug \
		"$pkgdir/usr/share/embloader/embloader.debug"
	install -d "$pkgdir/usr/share/doc/embloader/"
	cp -a docs/* "$pkgdir/usr/share/doc/embloader/"
	install -d "$pkgdir/usr/share/embloader/configs/"
	cp -a configs/* "$pkgdir/usr/share/embloader/configs/"
	if [ -f README.md ]; then install -m644 README.md "$pkgdir/usr/share/doc/embloader/"; fi
	if [ -f LICENSE ]; then install -m644 LICENSE "$pkgdir/usr/share/doc/embloader/"; fi
}

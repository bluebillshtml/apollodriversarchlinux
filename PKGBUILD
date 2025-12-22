# Maintainer: Apollo Driver Team <apollo-driver@noreply.github.com>
# Contributor: Community contributors
pkgname=apollo-driver
pkgver=0.1.0
pkgrel=1
pkgdesc="Linux driver for Universal Audio Apollo Twin Thunderbolt audio interfaces"
arch=('x86_64')
url="https://github.com/yourorg/apollo-driver"
license=('GPL2')
groups=('audio' 'drivers')
depends=(
    'linux'
    'linux-headers'
    'alsa-utils'
    'pipewire'
    'pipewire-alsa'
    'systemd'
)
makedepends=(
    'git'
    'make'
    'gcc'
    'dkms'
)
optdepends=(
    'pipewire-jack: JACK compatibility support'
    'jack2: Professional audio applications'
    'thunderbolt-tools: Thunderbolt device management'
)
provides=('apollo-driver')
conflicts=('apollo-driver-git' 'apollo-driver-dkms')
backup=('etc/apollo/apollo.conf')
install=apollo-driver.install
source=("apollo-driver-${pkgver}.tar.gz")
sha256sums=('SKIP')

prepare() {
    cd "${srcdir}/apollo-driver-${pkgver}"

    # Ensure we're using the correct kernel version
    _kernver=$(uname -r)
    sed -i "s|KDIR ?= /lib/modules/\$(shell uname -r)/build|KDIR ?= /usr/src/linux-headers-${_kernver}|g" kernel/Makefile
}

build() {
    cd "${srcdir}/apollo-driver-${pkgver}"

    # Build kernel module
    make -C kernel

    # Build user-space components
    make -C userspace

    # Build development tools
    make -C tools
}

package() {
    cd "${srcdir}/apollo-driver-${pkgver}"

    # Install using the project's install system
    make DESTDIR="${pkgdir}" install

    # Install DKMS configuration
    install -Dm644 dkms.conf "${pkgdir}/usr/src/apollo-driver-${pkgver}/dkms.conf"
    install -Dm644 kernel/Makefile "${pkgdir}/usr/src/apollo-driver-${pkgver}/Makefile"
    install -Dm644 kernel/*.c "${pkgdir}/usr/src/apollo-driver-${pkgver}/"
    install -Dm644 kernel/*.h "${pkgdir}/usr/src/apollo-driver-${pkgver}/"

    # Install udev rules
    install -Dm644 udev/99-apollo.rules "${pkgdir}/usr/lib/udev/rules.d/99-apollo.rules"

    # Install license
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    # Install additional documentation
    install -Dm644 docs/INSTALL.md "${pkgdir}/usr/share/doc/${pkgname}/INSTALL.md"
    install -Dm644 docs/USAGE.md "${pkgdir}/usr/share/doc/${pkgname}/USAGE.md"
    install -Dm644 docs/HACKING.md "${pkgdir}/usr/share/doc/${pkgname}/HACKING.md"
}

# vim:set ts=2 sw=2 et:

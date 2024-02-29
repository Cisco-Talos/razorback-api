extra_incl=
case "$host" in
    *-openbsd*)
        LDFLAGS="${LDFLAGS} -L/usr/local/lib -luuid"
        extra_incl="-I/usr/local/include"
	CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
    ;;
    *-linux*)
        CDEFINE="-D_GNU_SOURCE"
        CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE"
    ;;
    *-freebsd*)
        LDFLAGS="${LDFLAGS} -L/usr/local/lib -luuid"
        extra_incl="-I/usr/local/include"
        CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
    ;;
    *-apple*)
        AC_DEFINE(BROKEN_SIOCGIFMTU,,[Define if SIOCGIFMTU unavailable])
        e2fs_uuid=yes
        if test -d "/sw"; then
            LDFLAGS="${LDFLAGS} -L/sw/lib"
            extra_incl="-I/sw/include"
	    CPPFLAGS="${CPPFLAGS} -I/sw/include"
        fi
        if test -d "/opt/local"; then
            LDFLAGS="${LDFLAGS} -L/opt/local/lib"
            extra_incl="-I/opt/local/include"
	    CPPFLAGS="${CPPFLAGS} -I/opt/local/include"
        fi
        LIBTOOL=glibtool
        LIBTOOLIZE=glibtoolize
        CDEFINE="-Ds6_addr32=__u6_addr.__u6_addr32"
esac


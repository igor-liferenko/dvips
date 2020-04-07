all:
	gcc -DPACKAGE_NAME=\"dvipsk\ squeeze\" -DPACKAGE_TARNAME=\"dvipsk-squeeze\" -DPACKAGE_VERSION=\"5.97\" -DPACKAGE_STRING=\"dvipsk\ squeeze\ 5.97\" -DPACKAGE_BUGREPORT=\"tex-k@tug.org\" -DPACKAGE_URL=\"\" -DPACKAGE=\"dvipsk-squeeze\" -DVERSION=\"5.97\" -I. -I../../../../texk/dvipsk/squeeze   -Wdate-time -D_FORTIFY_SOURCE=2 -Wimplicit -Wreturn-type -g -O2 -fdebug-prefix-map=/usr/local/src/texlive-bin-2019.20190605.51237=. -fstack-protector-strong -Wformat -Werror=format-security -c -o squeeze.o ../../../../texk/dvipsk/squeeze/squeeze.c
	gcc -Wimplicit -Wreturn-type -g -O2 -fdebug-prefix-map=/usr/local/src/texlive-bin-2019.20190605.51237=. -fstack-protector-strong -Wformat -Werror=format-security  -Wl,-z,relro -o squeeze squeeze.o  
	echo timestamp >stamp-squeeze

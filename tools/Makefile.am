
bin_PROGRAMS = hangul hanjac

hangul_SOURCES = hangul.c
hangul_CFLAGS = -DLOCALEDIR=\"$(localedir)\"
hangul_LDADD = ../hangul/libhangul.la $(LTLIBINTL) $(LTLIBICONV)

hanjac_SOURCES = hanjac.c
hanjac_LDADD = ../hangul/libhangul.la $(LTLIBINTL) $(LTLIBICONV)

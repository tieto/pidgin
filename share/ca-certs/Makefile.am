CERTIFICATES = \
		CAcert_Root.pem \
		CAcert_Class3.pem \
		Entrust.net_Secure_Server_CA.pem \
		Entrust.net_2048.pem \
		Equifax_Secure_CA.pem \
		Equifax_Secure_Global_eBusiness_CA-1.pem \
		GTE_CyberTrust_Global_Root.pem \
		ValiCert_Class_2_VA.pem \
		mozilla.pem

#EXTRA_CERTS = \
#		filename.pem


cacertsdir =	$(datadir)/purple/ca-certs

if INSTALL_SSL_CERTIFICATES
cacerts_DATA = \
	$(CERTIFICATES) \
	$(EXTRA_CERTS)
else
cacerts_DATA = $(EXTRA_CERTS)
endif

EXTRA_DIST =	\
		Makefile.mingw \
		$(CERTIFICATES) \
		$(EXTRA_CERTS)

.PHONY: mk-ca-bundle.pl
mk-ca-bundle.pl:
	curl -o $@ https://raw.githubusercontent.com/curl/curl/HEAD/lib/mk-ca-bundle.pl
	chmod +x $@

.PHONY: mozilla.pem
mozilla.pem: mk-ca-bundle.pl
	./mk-ca-bundle.pl -u mozilla.pem

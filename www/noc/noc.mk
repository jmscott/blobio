
#
#  Variable NOC_DNS_SUFFIX determines the directory name for the the
#  web server, which matches the DNS name in the ssl certificate.  The
#  default is .com for building dash.setspace.com.  However, typically
#  in development the dns name will be local, like
#
#	dash.setspace.jmscott.cassimac.lan
#	dash.setspace.jmscott.tmonk.local
#
#  using a snake oil, self-signed ssl certificate built for development.
#
ifndef NOC_DNS_VHOST_SUFFIX
$(error NOC_DNS_VHOST_SUFFIX is not set)
endif
#
#  Note:
#	Rarely override:  for example, pgtalk.setspace.com
#
NOC_DNS_VHOST_PREFIX?=noc.blob

NOC_DNS_VHOST=$(NOC_DNS_VHOST_PREFIX).$(NOC_DNS_VHOST_SUFFIX)

WWW_PREFIX=$(BLOBIO_PREFIX)/www/vhost/$(NOC_DNS_VHOST)

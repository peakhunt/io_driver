#!/bin/bash

#
# generate a private key
#
openssl genrsa -out server.pem 2048

#
# generate CSR(Certificate Signing Request)
#
openssl req -new -key server.pem -out server.csr

#
# generate self signed cert
#
openssl x509 -req -days 365 -in server.csr -signkey server.pem -out server.crt

#
# validate cert just for a test
#
openssl x509 -in server.crt -text -noout

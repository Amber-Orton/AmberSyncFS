#!/bin/bash
# generate-certs.sh: Generate CA, server, and client certificates for mutual TLS
# Usage: ./generate-certs.sh
set -e

# Output directory
OUTDIR="certs"
mkdir -p "$OUTDIR"
cd "$OUTDIR"

# 1. Create CA key and certificate
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj "/CN=AmberSyncFS CA"

echo "[INFO] CA certificate and key generated."

# 2. Create server key and CSR
openssl genrsa -out server.key 4096
openssl req -new -key server.key -out server.csr -subj "/CN=AmberSyncFS Server"

# 3. Sign server CSR with CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 3650 -sha256

echo "[INFO] Server certificate and key generated."

# 4. Create client key and CSR
openssl genrsa -out client.key 4096
openssl req -new -key client.key -out client.csr -subj "/CN=AmberSyncFS Client"

# 5. Sign client CSR with CA
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 3650 -sha256

echo "[INFO] Client certificate and key generated."

echo "[SUCCESS] All certificates and keys are in the '$OUTDIR' directory."
echo "\nFiles generated:"
echo "  ca.crt, ca.key, server.crt, server.key, client.crt, client.key"
echo "\nDistribute as follows:"
echo "  Server: server.crt, server.key, ca.crt"
echo "  Client: client.crt, client.key, ca.crt"

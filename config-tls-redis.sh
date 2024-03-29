#!/bin/bash

# Variables
REDIS_CONF_PATH="./redis"
REDIS_CONF_PATH=${1:-"./redis"}
REDIS_CONF="${REDIS_CONF_PATH}/redis.conf"

# Create a directory for TLS files
mkdir -p "${REDIS_CONF_PATH}"

# Step 1: Generate CA, Server certificates, and keys
openssl req -x509 -newkey rsa:4096 -keyout "${REDIS_CONF_PATH}/ca_key.pem" -out "${REDIS_CONF_PATH}/ca_cert.pem" -days 365 -nodes -subj "/CN=ExampleCA"
openssl req -newkey rsa:4096 -nodes -keyout "${REDIS_CONF_PATH}/server_key.pem" -out "${REDIS_CONF_PATH}/server_req.pem" -subj "/CN=$(hostname)"
openssl x509 -req -in "${REDIS_CONF_PATH}/server_req.pem" -days 60 -CA "${REDIS_CONF_PATH}/ca_cert.pem" -CAkey "${REDIS_CONF_PATH}/ca_key.pem" -set_serial 01 -out "${REDIS_CONF_PATH}/server_cert.pem"

# Step 2: Create a Redis configuration file for TLS
cat > "${REDIS_CONF}" <<EOF
port 6380
tls-port 16380
tls-cert-file ${REDIS_CONF_PATH}/server_cert.pem
tls-key-file ${REDIS_CONF_PATH}/server_key.pem
tls-ca-cert-file ${REDIS_CONF_PATH}/ca_cert.pem
tls-auth-clients no
client-output-buffer-limit pubsub 512mb 256mb 60
save ""
EOF

# Step 3: Start Redis Server in TLS mode
echo "Run the following command to start the TLS secured Redis server"
echo redis-server "${REDIS_CONF}"

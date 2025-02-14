set -e
npm run build

cp -r node_modules/@reclaimprotocol/zk-symmetric-crypto/resources/ ./browser/resources
# remove r1cs files, we don't need them in prod
rm -rf ./browser/resources/snarkjs/*/*.r1cs
# remove gnark libs, they are only for nodejs
rm -rf ./browser/resources/gnark
npm exec webpack

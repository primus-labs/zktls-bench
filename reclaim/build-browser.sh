set -e
npm run build

mkdir -p ./browser/browser-rpc
cp -r node_modules/@reclaimprotocol/zk-symmetric-crypto/resources/ ./browser/browser-rpc/
# remove r1cs files, we don't need them in prod
rm -rf ./browser/resources/browser-rpc/snarkjs/*/*.r1cs
# remove gnark libs, they are only for nodejs
rm -rf ./browser/resources/browser-rpc/gnark
npm exec webpack

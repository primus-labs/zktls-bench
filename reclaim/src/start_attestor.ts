import { providers } from '@reclaimprotocol/attestor-core/lib/providers'
providers.http.additionalClientOptions = {
  // we need to disable certificate verification for testing purposes
  verifyServerCertificate: false,
  supportedProtocolVersions: ['TLS1_3']
}

const parseCertificate = require('@reclaimprotocol/tls/lib/utils/parse-certificate');
const mockVerifyCertificateChain = (): void => {
  // console.log('Mocked verifyCertificateChain called');
};
parseCertificate.verifyCertificateChain = mockVerifyCertificateChain

import { createServer } from '@reclaimprotocol/attestor-core/lib/server'



async function test() {
  // [port]
  const args = process.argv.slice(2)
  var attestorPort = 12345
  if (args.length > 0) {
    attestorPort = parseInt(args[0])
  }
  console.log('start simple attestor at:', attestorPort)

  const attestorServer = createServer(attestorPort)
}
test()

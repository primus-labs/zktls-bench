import { createMockServer } from './mock-provider-server'

const parseCertificate = require('@reclaimprotocol/tls/lib/utils/parse-certificate');
const mockVerifyCertificateChain = (): void => { };
parseCertificate.verifyCertificateChain = mockVerifyCertificateChain

import { createServer } from '@reclaimprotocol/attestor-core/lib/server'


async function test() {
  // [port]
  const args = process.argv.slice(2)
  var attestorPort = 12345
  if (args.length > 0) {
    attestorPort = parseInt(args[0])
  }

  // start embed-http-server for provider
  var httpsServerPort = 17777
  console.log('start simple provider at:', httpsServerPort)
  const mockHttpsServer = createMockServer(httpsServerPort)

  console.log('start simple attestor at:', attestorPort)
  const attestorServer = createServer(attestorPort)
}
test()

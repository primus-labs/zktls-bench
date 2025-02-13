import { providers } from '@reclaimprotocol/attestor-core/lib/providers'
providers.http.additionalClientOptions = {
  // we need to disable certificate verification for testing purposes
  verifyServerCertificate: false,
  supportedProtocolVersions: ['TLS1_3']
}
import { createMockServer } from './mock-provider-server'

async function test() {
  var httpsServerPort = 17777
  console.log('start simple provider at:', httpsServerPort)
  const mockHttpsServer = createMockServer(httpsServerPort)
}
void test()

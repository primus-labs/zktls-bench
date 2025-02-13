import { providers } from '@reclaimprotocol/attestor-core/lib/providers'
providers.http.additionalClientOptions = {
  // we need to disable certificate verification for testing purposes
  verifyServerCertificate: false,
  supportedProtocolVersions: ['TLS1_3']
}

const parseCertificate = require('@reclaimprotocol/tls/lib/utils/parse-certificate');
const mockVerifyCertificateChain = (): void => {
  console.log('Mocked verifyCertificateChain called');
};
parseCertificate.verifyCertificateChain = mockVerifyCertificateChain

import { createClaimOnAttestor } from '@reclaimprotocol/attestor-core/lib/client'
import { ZKEngine } from '@reclaimprotocol/zk-symmetric-crypto'
import { AttestorClient } from '@reclaimprotocol/attestor-core/lib/client/utils/client-socket'
import { logger, uint8ArrayToStr } from '@reclaimprotocol/attestor-core/lib/utils'
import { createMockServer } from './mock-provider-server'


export async function startProver(
  attestorIp: string = 'localhost', attestorPort: number = 12345,
  zkEngine: ZKEngine = 'gnark', reqLength: number = 1024, repLength: number = 1024,
  mockUrl = 'https://localhost:17777/me') {
  // console.log("process.memoryUsage1", process.memoryUsage());
  // console.log("process.resourceUsage1", process.resourceUsage());

  const wsServerUrl = `ws://${attestorIp}:${attestorPort}/ws`
  const client = new AttestorClient({
    logger: logger.child({ client: 1 }),
    url: wsServerUrl,
  })
  await client.waitForInit()

  const user = 'adhiraj'
  const claimUrl = mockUrl

  const data = uint8ArrayToStr(new Uint8Array(reqLength))
  const start = +new Date()
  const result = await createClaimOnAttestor({
    name: 'http',
    params: {
      url: claimUrl,
      method: 'POST',
      body: data,
      headers: {
        'replength': repLength.toString()
      },
      responseRedactions: [],
      responseMatches: [
        {
          type: 'contains',
          value: `${user}@mock.com`
        }
      ]
    },
    secretParams: {
      authorisationHeader: `Bearer ${user}`
    },
    ownerPrivateKey: '0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80',
    client: client,
    zkEngine,
  })
  // console.log('claimUrl', claimUrl)
  // console.log('elapsed', +new Date() - start)

  var memeory = 0
  if (typeof window === 'undefined') {
    memeory = process.memoryUsage().rss / 1024
  }

  const stat = {
    ak: zkEngine,
    cost: +new Date() - start,
    memory: memeory
  }
  // console.log("process.memoryUsage2", process.memoryUsage());
  // console.log("process.resourceUsage2", process.resourceUsage());
  console.log(`DONE:${JSON.stringify(stat)}`);

  await client.terminateConnection()
}

async function test() {
  var attestorIp = "localhost"
  var attestorPort = 12345
  var zkEngine: ZKEngine = 'gnark'
  var reqLength = 1024
  var repLength = 1024
  var mockUrl = `https://localhost:17777/me`
  if (typeof window !== 'undefined') {
    attestorIp = "127.0.0.1"
    attestorPort = 12345
    zkEngine = 'snarkjs'
    reqLength = 1024
    repLength = 1024
    // note: if run desktop-browser-chrome.
    // - use `python https_server.py` start the server 
    // - set `–unsafely-treat-insecure-origin-as-secure=ws://127.0.0.1:12345`
    mockUrl = `https://127.0.0.1:17777/me`
    // on browser
    console.log('on browser, see browser/index.html')
    return;
  } else {
    // on node
    // [ip] [port] [gnark|snarkjs] [request-length] [response-length]
    const args = process.argv.slice(2)
    if (args.length > 0) {
      attestorIp = args[0]
    }
    if (args.length > 1) {
      attestorPort = parseInt(args[1])
    }
    if (args.length > 2) {
      if (args[2] == 'snarkjs') zkEngine = 'snarkjs';
    }
    if (args.length > 3) {
      reqLength = parseInt(args[3])
    }
    if (args.length > 4) {
      repLength = parseInt(args[4])
    }
  }

  await startProver(attestorIp, attestorPort, zkEngine, reqLength, repLength, mockUrl)

}
test()


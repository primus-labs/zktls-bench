/**
 * Mock HTTPS server to implement the "mock-login" provider
 */
import { readFileSync } from 'fs'
import { createServer } from 'https'
import { logger, uint8ArrayToStr } from '@reclaimprotocol/attestor-core/lib/utils'

import { defaultData } from './comm'

/**
 * Mock https server to test claim creation.
 * It implements a GET /me endpoint that returns the email address
 * of the user. A bearer token is expected in the Authorization header.
 *
 * The bearer token is expected to be the email address without the domain.
 * Eg. to claim the email address "abcd@mock.com", the header
 * should be "Authorization: Bearer abcd".
 */
export function createMockServer(port: number) {
  const tlsSessionStore: Record<string, Buffer> = {}

  const server = createServer(
    {
      key: readFileSync('./cert/private-key.pem'),
      cert: readFileSync('./cert/public-cert.pem'),
    }
  )

  server.on('request', (req, res) => {
    // console.log('req',req)
    if (req.method !== 'POST') {
      endWithError(405, 'invalid method')
      return
    }

    var repLength = 1
    req.on('data', (chunk) => {
      console.log('req.headers', req.headers)
      console.log('headers.length', Buffer.byteLength(JSON.stringify(req.headers)))
      console.log('chunk.length', chunk.length)
      repLength = parseInt(req.headers['replength'] as string)
      console.log('repLength', repLength)
    });

    if (!req.url?.startsWith('/me')) {
      endWithError(404, 'invalid path')
      return
    }

    const auth = req.headers.authorization
    if (!auth) {
      endWithError(401, 'missing authorization header')
      return
    }

    if (!auth?.startsWith('Bearer ')) {
      endWithError(401, 'invalid authorization header')
      return
    }

    const emailAddress = auth.slice('Bearer '.length) + '@mock.com'
    endWithJson(200, { emailAddress })

    logger.info({ emailAddress }, 'ended with success')

    function endWithError(status: number, message: string) {
      endWithJson(status, { error: message })

      logger.info({ status, message }, 'ended with error')
    }

    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    function endWithJson(status: number, json: any) {
      repLength = parseInt(req.headers['replength'] as string)
      // const str = JSON.stringify(json) + uint8ArrayToStr(new Uint8Array(repLength))
      // const str = `{"d":${uint8ArrayToStr(new Uint8Array(repLength))}}`
      // const str = `{"d":"${uint8ArrayToStr(new Uint8Array(repLength))}"}`
      // const str = `{"t":"t","d":"${defaultData.slice(0, repLength)}"}`
      const str = `{"d":"${defaultData.slice(0, repLength)}"}`
      console.log('str.length', str.length)
      // console.log('str', str)
      res.writeHead(status, {
        'Content-Type': 'application/json',
        'Content-Length': str.length.toString(),
      })
      res.write(str)
      res.end()
    }
  })

  server.listen(port)

  return { server, tlsSessionStore }
}
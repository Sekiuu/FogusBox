import { createRequire } from 'node:module'
const require = createRequire(import.meta.url)
function createRemoteCollection(fetchEndpoint) {
  let _cache
  return async () => {
    if (_cache)
      return _cache
    _cache = fetch(fetchEndpoint).then(r => r.json()).then((data) => {
      _cache = data
      return data
    }).catch((error) => {
      _cache = undefined
      throw error
    })
    return _cache
  }
}

export const collections = {
}
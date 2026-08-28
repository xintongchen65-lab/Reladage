import type { MoleSessionConfig } from '../types/result'
import type { MoleMotionDataSource } from './contracts'
import { MoleFakeDataSource } from './fake-data-source'
import { MoleRealDataSource, type MoleRealTransport } from './real-data-source'

let realTransport: MoleRealTransport = { subscribe: () => () => {} }
export function installMoleRealTransport(transport: MoleRealTransport): void { realTransport = transport }
export function createMoleMotionDataSource(config: MoleSessionConfig): MoleMotionDataSource {
  return config.sourceKind === 'real' ? new MoleRealDataSource(realTransport) : new MoleFakeDataSource(config)
}

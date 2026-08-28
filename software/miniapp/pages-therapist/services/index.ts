import type { TherapistRepository } from './repository'
import { mockRepository } from './mock-repository'

let currentRepository: TherapistRepository = mockRepository

export function getTherapistRepository(): TherapistRepository {
  return currentRepository
}

export function setTherapistRepository(repository: TherapistRepository): void {
  currentRepository = repository
}

export type { TherapistRepository } from './repository'

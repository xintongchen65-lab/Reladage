import type {
  AiSuggestion,
  AlertItem,
  DispatchResult,
  PatientDetail,
  PatientSummary,
  PlanPatch,
  TherapistDashboard,
  TherapistProfile
} from '../types'

export interface TherapistRepository {
  getDashboard(): Promise<TherapistDashboard>
  listPatients(): Promise<PatientSummary[]>
  getPatientDetail(patientId: string): Promise<PatientDetail>
  listAlerts(): Promise<AlertItem[]>
  getAlert(alertId: string): Promise<AlertItem | null>
  listAiSuggestions(): Promise<AiSuggestion[]>
  getAiSuggestion(id: string): Promise<AiSuggestion | null>
  approveAiSuggestion(id: string, patch?: PlanPatch): Promise<DispatchResult>
  getProfile(): Promise<TherapistProfile>
}

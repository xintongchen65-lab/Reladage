const MEMBERS_KEY = 'rehabmotion_family_members_v1'
const CURRENT_MEMBER_KEY = 'rehabmotion_current_member_v1'

const seedMembers = [{
  id: 'father-wang',
  name: '王建国',
  relationship: '父亲',
  stage: '膝关节术后 · 第4周',
  createdAt: '2026-08-01'
}]

const todayISO = () => {
  const date = new Date()
  const month = String(date.getMonth() + 1).padStart(2, '0')
  const day = String(date.getDate()).padStart(2, '0')
  return `${date.getFullYear()}-${month}-${day}`
}

const normalizeMember = member => ({
  id: String(member.id || `member-${Date.now()}`),
  name: String(member.name || '').trim(),
  relationship: String(member.relationship || '家人').trim(),
  stage: String(member.stage || '居家康复训练').trim(),
  createdAt: String(member.createdAt || todayISO())
})

export function getMembers() {
  const stored = uni.getStorageSync(MEMBERS_KEY)
  return Array.isArray(stored) && stored.length ? stored.map(normalizeMember) : seedMembers.map(normalizeMember)
}

export function getCurrentMember() {
  const members = getMembers()
  const currentId = uni.getStorageSync(CURRENT_MEMBER_KEY)
  return members.find(item => item.id === currentId) || members[0]
}

export function getCurrentMemberLabel() {
  const member = getCurrentMember()
  return `${member.relationship} · ${member.name}`
}

export function getCurrentMemberId() {
  return getCurrentMember().id
}

export function setCurrentMember(memberId) {
  const member = getMembers().find(item => item.id === memberId)
  if (!member) return false
  uni.setStorageSync(CURRENT_MEMBER_KEY, member.id)
  return true
}

export function addMember(input) {
  const member = normalizeMember({ ...input, id: `member-${Date.now()}` })
  if (!member.name) throw new Error('请填写成员姓名')
  const members = [...getMembers(), member]
  uni.setStorageSync(MEMBERS_KEY, members)
  uni.setStorageSync(CURRENT_MEMBER_KEY, member.id)
  return member
}

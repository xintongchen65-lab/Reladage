create table if not exists app_users (
  id uuid primary key,
  wechat_openid text not null unique,
  wechat_unionid text unique,
  display_name varchar(80) not null default '微信用户',
  status varchar(20) not null default 'active',
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create table if not exists family_members (
  id uuid primary key,
  name varchar(40) not null,
  relationship varchar(20) not null,
  stage varchar(120) not null,
  created_by uuid not null references app_users(id),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  archived_at timestamptz
);

create table if not exists member_permissions (
  user_id uuid not null references app_users(id) on delete cascade,
  member_id uuid not null references family_members(id) on delete cascade,
  role varchar(20) not null check (role in ('elder', 'family', 'doctor', 'therapist')),
  created_at timestamptz not null default now(),
  primary key (user_id, member_id)
);

create table if not exists devices (
  id uuid primary key,
  serial_number varchar(80) not null unique,
  device_name varchar(80) not null,
  firmware_version varchar(80),
  last_status jsonb not null default '{}'::jsonb,
  last_seen_at timestamptz,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create table if not exists member_devices (
  member_id uuid not null references family_members(id) on delete cascade,
  device_id uuid not null references devices(id) on delete cascade,
  active boolean not null default true,
  bound_at timestamptz not null default now(),
  unbound_at timestamptz,
  primary key (member_id, device_id)
);

create unique index if not exists one_active_device_per_member
  on member_devices(member_id) where active = true;

create table if not exists training_plans (
  id uuid primary key,
  member_id uuid not null references family_members(id) on delete cascade,
  revision integer not null,
  schema_version integer not null,
  payload jsonb not null,
  created_by uuid not null references app_users(id),
  created_at timestamptz not null default now(),
  unique (member_id, revision)
);

create table if not exists training_sessions (
  id uuid primary key,
  member_id uuid not null references family_members(id) on delete cascade,
  external_session_id varchar(120) not null,
  training_date date not null,
  started_at timestamptz,
  ended_at timestamptz,
  status varchar(24) not null check (status in ('completed', 'partial', 'not_started', 'cancelled')),
  planned boolean not null default true,
  completion numeric(5,2) not null default 0 check (completion between 0 and 100),
  qualified numeric(5,2) not null default 0 check (qualified between 0 and 100),
  minutes integer not null default 0 check (minutes >= 0),
  max_angle numeric(6,2) not null default 0,
  completed_reps integer not null default 0 check (completed_reps >= 0),
  qualified_reps integer not null default 0 check (qualified_reps >= 0),
  planned_reps integer not null default 0 check (planned_reps >= 0),
  reasons jsonb not null default '{}'::jsonb,
  interrupted integer not null default 0 check (interrupted >= 0),
  source varchar(40) not null default 'device',
  items jsonb not null default '[]'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (member_id, external_session_id)
);

create index if not exists training_sessions_member_date
  on training_sessions(member_id, training_date desc);

create table if not exists training_session_items (
  id uuid primary key,
  session_id uuid not null references training_sessions(id) on delete cascade,
  item_order integer not null,
  payload jsonb not null,
  created_at timestamptz not null default now(),
  unique (session_id, item_order)
);

create table if not exists messages (
  id uuid primary key,
  member_id uuid not null references family_members(id) on delete cascade,
  message_key varchar(160) not null,
  type varchar(30) not null,
  title varchar(120) not null,
  content text not null,
  route varchar(240),
  created_at timestamptz not null default now(),
  unique (member_id, message_key)
);

create table if not exists message_reads (
  user_id uuid not null references app_users(id) on delete cascade,
  member_id uuid not null references family_members(id) on delete cascade,
  message_id varchar(160) not null,
  read_at timestamptz not null default now(),
  primary key (user_id, member_id, message_id)
);

<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<meta name="description" content="PRATIKSHA — Hospital Management System" />
<title>PRATIKSHA — Hospital Management System</title>

<link rel="preconnect" href="https://fonts.googleapis.com" />
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet" />

<style>
/* ============================================================
   PRATIKSHA — Frontend Styles
   ============================================================ */

*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

:root {
    --bg:              #F5F7FA;
    --card:            #FFFFFF;
    --primary:         #2563EB;
    --primary-hover:   #1D4ED8;
    --primary-soft:    #EFF6FF;
    --primary-soft-2:  #DBEAFE;
    --secondary:       #0D9488;
    --secondary-soft:  #F0FDFA;
    --violet:          #7C3AED;
    --violet-soft:     #F5F3FF;
    --text:            #0F172A;
    --text-2:          #475569;
    --text-3:          #64748B;
    --text-4:          #94A3B8;
    --border:          #E2E8F0;
    --border-2:        #F1F5F9;
    --green:           #16A34A;
    --green-soft:      #F0FDF4;
    --green-border:    #BBF7D0;
    --red:             #DC2626;
    --red-soft:        #FEF2F2;
    --red-border:      #FECACA;
    --amber:           #D97706;
    --amber-soft:      #FFFBEB;
    --amber-border:    #FDE68A;
    --radius-sm:       10px;
    --radius:          14px;
    --radius-lg:       18px;
    --radius-xl:       22px;
    --shadow-sm:       0 1px 2px rgba(15,23,42,0.04);
    --shadow:          0 4px 16px rgba(15,23,42,0.06);
    --shadow-lg:       0 16px 40px rgba(15,23,42,0.10);
    --sidebar-w:       260px;
    --ease:            0.18s ease;
}

html, body { height: 100%; }

body {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
    font-size: 14px;
    line-height: 1.5;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
}

button { font-family: inherit; }
.icon { flex-shrink: 0; display: inline-block; vertical-align: middle; }

/* Sidebar */
.app { min-height: 100vh; }

.sidebar {
    position: fixed;
    top: 0; left: 0; bottom: 0;
    width: var(--sidebar-w);
    background: var(--card);
    border-right: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    z-index: 40;
    transition: transform 0.25s ease;
}

.sidebar-brand {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 22px 20px;
    border-bottom: 1px solid var(--border-2);
}

.brand-mark {
    width: 42px; height: 42px;
    border-radius: 13px;
    background: linear-gradient(135deg, var(--primary) 0%, #3B82F6 100%);
    color: #fff;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 800;
    font-size: 19px;
    letter-spacing: -0.02em;
    box-shadow: 0 4px 14px rgba(37,99,235,0.32);
    flex-shrink: 0;
}

.brand-text { display: flex; flex-direction: column; min-width: 0; }
.brand-name { font-weight: 700; font-size: 14px; letter-spacing: 0.06em; color: var(--text); }
.brand-sub { font-size: 12px; color: var(--text-3); font-weight: 500; }

.sidebar-nav {
    flex: 1;
    overflow-y: auto;
    padding: 14px 12px 8px;
    display: flex;
    flex-direction: column;
    gap: 2px;
}

.nav-section-label {
    font-size: 10.5px;
    font-weight: 700;
    letter-spacing: 0.09em;
    text-transform: uppercase;
    color: var(--text-4);
    padding: 10px 12px 6px;
}

.nav-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px 12px;
    border-radius: var(--radius-sm);
    color: var(--text-2);
    font-weight: 500;
    font-size: 13.5px;
    cursor: pointer;
    border: none;
    background: transparent;
    width: 100%;
    text-align: left;
    transition: background var(--ease), color var(--ease);
}
.nav-item:hover { background: #F8FAFC; color: var(--text); }
.nav-item.active { background: var(--primary-soft); color: var(--primary); font-weight: 600; }
.nav-item.active .icon { color: var(--primary); }
.nav-item .icon { color: var(--text-3); transition: color var(--ease); }
.nav-item:hover .icon { color: var(--text-2); }

.sidebar-status {
    margin: 12px;
    padding: 14px;
    border-radius: var(--radius);
    background: #F8FAFC;
    border: 1px solid var(--border-2);
}
.status-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 8px;
    gap: 8px;
}
.status-label {
    font-size: 10.5px;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-3);
}
.status-pill {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 4px 9px;
    border-radius: 999px;
    font-size: 11px;
    font-weight: 600;
    background: var(--green-soft);
    color: var(--green);
    border: 1px solid var(--green-border);
    white-space: nowrap;
}
.status-pill.pending {
    background: var(--amber-soft);
    color: var(--amber);
    border-color: var(--amber-border);
}
.status-pill.disconnected {
    background: var(--red-soft);
    color: var(--red);
    border-color: var(--red-border);
}
.status-pill.pending .dot-live { animation: none; }
.status-pill.disconnected .dot-live { animation: none; box-shadow: none; }

.dot-live {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: currentColor;
    animation: pulse 2s infinite;
}
@keyframes pulse {
    0%   { box-shadow: 0 0 0 0 rgba(22,163,74,0.45); }
    70%  { box-shadow: 0 0 0 6px rgba(22,163,74,0); }
    100% { box-shadow: 0 0 0 0 rgba(22,163,74,0); }
}

.status-detail { font-size: 12px; color: var(--text-3); line-height: 1.4; }

/* Main */
.main {
    margin-left: var(--sidebar-w);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
}

.topbar {
    position: sticky;
    top: 0;
    background: rgba(255,255,255,0.85);
    backdrop-filter: saturate(180%) blur(12px);
    -webkit-backdrop-filter: saturate(180%) blur(12px);
    border-bottom: 1px solid var(--border);
    padding: 14px 28px;
    display: flex;
    align-items: center;
    gap: 20px;
    z-index: 30;
}

.menu-btn { display: none; }

.topbar-title h1 {
    font-size: 16px;
    font-weight: 700;
    color: var(--text);
    letter-spacing: -0.01em;
    line-height: 1.3;
}
.topbar-sub {
    display: block;
    font-size: 12px;
    color: var(--text-3);
    font-weight: 500;
}

.topbar-search {
    margin-left: auto;
    position: relative;
    max-width: 340px;
    flex: 1;
}
.topbar-search .icon {
    position: absolute;
    left: 12px;
    top: 50%;
    transform: translateY(-50%);
    color: var(--text-4);
    pointer-events: none;
}
.topbar-search input {
    width: 100%;
    padding: 9px 12px 9px 38px;
    border-radius: 10px;
    border: 1px solid var(--border);
    background: #F8FAFC;
    font-size: 13px;
    font-family: inherit;
    color: var(--text);
    transition: border-color var(--ease), background var(--ease), box-shadow var(--ease);
}
.topbar-search input:focus {
    outline: none;
    border-color: var(--primary);
    background: #fff;
    box-shadow: 0 0 0 3px rgba(37,99,235,0.12);
}
.topbar-search input::placeholder { color: var(--text-4); }

.topbar-actions { display: flex; align-items: center; gap: 14px; }
.topbar-date {
    font-size: 12.5px;
    color: var(--text-3);
    font-weight: 500;
    white-space: nowrap;
}

.icon-btn {
    width: 36px; height: 36px;
    border-radius: 10px;
    border: 1px solid var(--border);
    background: #fff;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    color: var(--text-2);
    cursor: pointer;
    position: relative;
    transition: background var(--ease), border-color var(--ease), color var(--ease);
}
.icon-btn:hover { background: #F8FAFC; color: var(--text); }
.icon-btn .dot {
    position: absolute;
    top: 7px; right: 8px;
    width: 7px; height: 7px;
    border-radius: 50%;
    background: var(--red);
    border: 2px solid #fff;
}

.avatar {
    width: 36px; height: 36px;
    border-radius: 10px;
    background: linear-gradient(135deg, #0F172A 0%, #334155 100%);
    color: #fff;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    font-weight: 600;
    font-size: 12.5px;
    letter-spacing: 0.02em;
}

/* Content */
.content {
    padding: 28px;
    max-width: 1440px;
    width: 100%;
    margin: 0 auto;
    flex: 1;
}

.page { display: none; animation: fadeIn 0.22s ease; }
.page.active { display: block; }
@keyframes fadeIn {
    from { opacity: 0; transform: translateY(4px); }
    to   { opacity: 1; transform: none; }
}

/* Welcome */
.welcome {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 24px;
    padding: 28px;
    border-radius: var(--radius-xl);
    background: linear-gradient(135deg, #FFFFFF 0%, #F8FAFC 100%);
    border: 1px solid var(--border);
    margin-bottom: 20px;
    flex-wrap: wrap;
    box-shadow: var(--shadow-sm);
}
.welcome-text { flex: 1; min-width: 240px; }
.welcome-eyebrow {
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 0.09em;
    text-transform: uppercase;
    color: var(--primary);
    margin-bottom: 8px;
}
.welcome-title {
    font-size: 24px;
    font-weight: 700;
    color: var(--text);
    letter-spacing: -0.02em;
    margin-bottom: 6px;
    line-height: 1.25;
}
.welcome-sub { font-size: 14px; color: var(--text-3); max-width: 540px; }
.welcome-actions { display: flex; gap: 10px; flex-wrap: wrap; }

/* Buttons */
.btn {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 10px 16px;
    border-radius: 10px;
    font-size: 13.5px;
    font-weight: 600;
    font-family: inherit;
    cursor: pointer;
    border: 1px solid transparent;
    transition: all var(--ease);
    white-space: nowrap;
    text-decoration: none;
}
.btn-primary {
    background: var(--primary);
    color: #fff;
    box-shadow: 0 2px 8px rgba(37,99,235,0.24);
}
.btn-primary:hover { background: var(--primary-hover); box-shadow: 0 4px 14px rgba(37,99,235,0.32); }
.btn-secondary {
    background: #fff;
    color: var(--text);
    border-color: var(--border);
    box-shadow: var(--shadow-sm);
}
.btn-secondary:hover { background: #F8FAFC; border-color: #CBD5E1; }
.btn-ghost {
    background: transparent;
    color: var(--primary);
    border-color: transparent;
    padding: 6px 10px;
    font-size: 13px;
}
.btn-ghost:hover { background: var(--primary-soft); }
.btn:disabled { opacity: 0.55; cursor: not-allowed; }

/* Stats grid */
.stats-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 16px;
    margin-bottom: 24px;
}
.stat-card {
    background: #fff;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px;
    transition: box-shadow var(--ease), transform var(--ease), border-color var(--ease);
}
.stat-card:hover {
    box-shadow: var(--shadow);
    transform: translateY(-1px);
    border-color: #CBD5E1;
}
.stat-top { display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 14px; }
.stat-icon {
    width: 42px; height: 42px;
    border-radius: 12px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
}
.stat-icon.blue   { background: var(--primary-soft);   color: var(--primary); }
.stat-icon.teal   { background: var(--secondary-soft); color: var(--secondary); }
.stat-icon.violet { background: var(--violet-soft);    color: var(--violet); }
.stat-icon.amber  { background: var(--amber-soft);     color: var(--amber); }

.stat-value {
    font-size: 28px;
    font-weight: 700;
    color: var(--text);
    letter-spacing: -0.02em;
    line-height: 1.1;
    margin-bottom: 4px;
}
.stat-label { font-size: 13px; color: var(--text-3); font-weight: 500; }
.stat-hint {
    font-size: 11.5px;
    color: var(--text-4);
    margin-top: 8px;
    display: flex;
    align-items: center;
    gap: 4px;
    font-weight: 500;
}
.stat-hint.warn { color: var(--amber); }
.stat-hint.ok   { color: var(--green); }

/* Sections */
.section { margin-bottom: 24px; }
.section-head {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 12px;
    margin-bottom: 12px;
}
.section-title { font-size: 15px; font-weight: 700; color: var(--text); letter-spacing: -0.01em; }
.section-sub   { font-size: 13px; color: var(--text-3); margin-top: 2px; }

/* Quick actions */
.actions-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 16px;
}
.action-card {
    background: #fff;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px;
    cursor: pointer;
    transition: all var(--ease);
    text-align: left;
    font-family: inherit;
}
.action-card:hover {
    border-color: var(--primary);
    box-shadow: var(--shadow);
    transform: translateY(-1px);
}
.action-icon {
    width: 38px; height: 38px;
    border-radius: 10px;
    background: var(--primary-soft);
    color: var(--primary);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 14px;
}
.action-title { font-size: 13.5px; font-weight: 600; color: var(--text); margin-bottom: 4px; }
.action-desc  { font-size: 12.5px; color: var(--text-3); line-height: 1.45; }

/* Card */
.card {
    background: #fff;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    overflow: hidden;
}
.card-body { padding: 20px; }

/* Table */
.table-wrap { overflow-x: auto; }
.data-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 13.5px;
}
.data-table thead th {
    text-align: left;
    font-size: 11.5px;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--text-3);
    padding: 12px 20px;
    background: #F8FAFC;
    border-bottom: 1px solid var(--border);
    white-space: nowrap;
}
.data-table tbody td {
    padding: 14px 20px;
    border-bottom: 1px solid var(--border-2);
    color: var(--text-2);
    vertical-align: middle;
}
.data-table tbody tr:last-child td { border-bottom: none; }
.data-table tbody tr:hover { background: #FAFBFC; }
.cell-strong { color: var(--text); font-weight: 600; }

.test-id {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    min-width: 28px;
    height: 28px;
    padding: 0 8px;
    border-radius: 8px;
    background: var(--primary-soft);
    color: var(--primary);
    font-weight: 700;
    font-size: 12px;
}

/* Badges */
.badge {
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 4px 10px;
    border-radius: 999px;
    font-size: 11.5px;
    font-weight: 600;
    border: 1px solid transparent;
    white-space: nowrap;
}
.badge-green { background: var(--green-soft); color: var(--green); border-color: var(--green-border); }
.badge-grey  { background: #F1F5F9; color: var(--text-3); border-color: var(--border); }
.badge-amber { background: var(--amber-soft); color: var(--amber); border-color: var(--amber-border); }

/* States */
.loading-state {
    padding: 48px 24px;
    text-align: center;
    color: var(--text-3);
    font-size: 13.5px;
}
.spinner {
    width: 24px; height: 24px;
    border: 2.5px solid var(--border);
    border-top-color: var(--primary);
    border-radius: 50%;
    animation: spin 0.7s linear infinite;
    margin: 0 auto 12px;
}
@keyframes spin { to { transform: rotate(360deg); } }

.empty-state { padding: 48px 24px; text-align: center; }
.empty-icon {
    width: 52px; height: 52px;
    border-radius: 14px;
    background: #F1F5F9;
    color: var(--text-3);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 14px;
}
.empty-title { font-size: 14px; font-weight: 600; color: var(--text); margin-bottom: 4px; }
.empty-desc  { font-size: 13px; color: var(--text-3); max-width: 380px; margin: 0 auto; line-height: 1.55; }

.error-state { padding: 40px 24px; text-align: center; }
.error-icon {
    width: 52px; height: 52px;
    border-radius: 14px;
    background: var(--red-soft);
    color: var(--red);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    margin-bottom: 14px;
}
.error-title { font-size: 14px; font-weight: 600; color: var(--text); margin-bottom: 4px; }
.error-desc  { font-size: 13px; color: var(--text-3); margin-bottom: 16px; max-width: 400px; margin-left: auto; margin-right: auto; }

/* Key-value list */
.kv-list { display: grid; gap: 0; }
.kv-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 12px 0;
    border-bottom: 1px solid var(--border-2);
    gap: 12px;
}
.kv-row:last-child { border-bottom: none; }
.kv-key { font-size: 13px; color: var(--text-3); font-weight: 500; }
.kv-val { font-size: 13px; color: var(--text); font-weight: 600; text-align: right; }

/* Forms */
.form-card {
    background: #fff;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 22px;
    margin-bottom: 20px;
    display: none;
}
.form-card.open { display: block; animation: fadeIn 0.22s ease; }
.form-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 16px;
}
.form-field { display: flex; flex-direction: column; gap: 6px; }
.form-field.full { grid-column: 1 / -1; }
.form-label { font-size: 12.5px; font-weight: 600; color: var(--text-2); }
.form-input,
.form-select,
.form-textarea {
    padding: 10px 12px;
    border-radius: 10px;
    border: 1px solid var(--border);
    background: #fff;
    font-size: 13.5px;
    font-family: inherit;
    color: var(--text);
    transition: border-color var(--ease), box-shadow var(--ease);
    width: 100%;
}
.form-input:focus,
.form-select:focus,
.form-textarea:focus {
    outline: none;
    border-color: var(--primary);
    box-shadow: 0 0 0 3px rgba(37,99,235,0.12);
}
.form-textarea { resize: vertical; min-height: 84px; }
.form-actions {
    display: flex;
    gap: 10px;
    margin-top: 20px;
    justify-content: flex-end;
    flex-wrap: wrap;
}
.form-note {
    font-size: 12.5px;
    color: var(--amber);
    background: var(--amber-soft);
    border: 1px solid var(--amber-border);
    padding: 10px 12px;
    border-radius: 10px;
    margin-top: 16px;
    display: flex;
    align-items: flex-start;
    gap: 8px;
    line-height: 1.5;
}

/* Toolbar */
.toolbar {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-bottom: 16px;
    flex-wrap: wrap;
}
.search-box { position: relative; flex: 1; max-width: 380px; min-width: 200px; }
.search-box .icon {
    position: absolute;
    left: 12px;
    top: 50%;
    transform: translateY(-50%);
    color: var(--text-4);
    pointer-events: none;
}
.search-box input {
    width: 100%;
    padding: 10px 12px 10px 38px;
    border-radius: 10px;
    border: 1px solid var(--border);
    background: #fff;
    font-size: 13.5px;
    font-family: inherit;
    color: var(--text);
    transition: border-color var(--ease), box-shadow var(--ease);
}
.search-box input:focus {
    outline: none;
    border-color: var(--primary);
    box-shadow: 0 0 0 3px rgba(37,99,235,0.12);
}

/* Page header */
.page-header {
    margin-bottom: 20px;
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    gap: 16px;
    flex-wrap: wrap;
}
.page-header h2 {
    font-size: 20px;
    font-weight: 700;
    letter-spacing: -0.02em;
    color: var(--text);
    margin-bottom: 4px;
}
.page-header p { font-size: 13.5px; color: var(--text-3); }
.page-header-actions { display: flex; gap: 10px; flex-wrap: wrap; }

/* Toast */
.toast {
    position: fixed;
    bottom: 24px;
    right: 24px;
    background: #1E293B;
    color: #fff;
    padding: 12px 16px;
    border-radius: 12px;
    font-size: 13px;
    font-weight: 500;
    box-shadow: var(--shadow-lg);
    z-index: 100;
    display: flex;
    align-items: center;
    gap: 10px;
    max-width: 380px;
    animation: toastIn 0.22s ease;
    line-height: 1.5;
}
.toast.warn  { background: #B45309; }
.toast.error { background: #B91C1C; }
@keyframes toastIn {
    from { opacity: 0; transform: translateY(10px); }
    to   { opacity: 1; transform: none; }
}

/* Mini stats */
.mini-stats {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 16px;
    margin-bottom: 20px;
}
.mini-stat {
    background: #fff;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 18px 20px;
}
.mini-stat-label { font-size: 12.5px; color: var(--text-3); font-weight: 500; margin-bottom: 6px; }
.mini-stat-value { font-size: 22px; font-weight: 700; color: var(--text); letter-spacing: -0.02em; }

/* Overlay */
.sidebar-overlay { display: none; }

/* ------------------------------------------------------------
   DIAGNOSTIC QUEUE
   ------------------------------------------------------------ */
.queue-grid {
    display: grid;
    grid-template-columns: 380px 1fr;
    gap: 20px;
    align-items: start;
}
@media (max-width: 1024px) {
    .queue-grid { grid-template-columns: 1fr; }
}

.test-picker {
    display: grid;
    gap: 8px;
}
.test-pick {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px 12px;
    border: 1px solid var(--border);
    border-radius: 10px;
    cursor: pointer;
    transition: all var(--ease);
    font-size: 13px;
}
.test-pick:hover { border-color: var(--primary); background: #FAFBFC; }
.test-pick input { accent-color: var(--primary); cursor: pointer; }
.test-pick.checked { border-color: var(--primary); background: var(--primary-soft); }

.centre-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 12px;
}
@media (max-width: 720px) {
    .centre-grid { grid-template-columns: 1fr; }
}

.centre-card {
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 16px;
    background: #fff;
}
.centre-head {
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    margin-bottom: 10px;
    gap: 10px;
}
.centre-name { font-weight: 600; font-size: 13.5px; color: var(--text); }
.centre-sub  { font-size: 11px; color: var(--text-3); margin-top: 2px; }
.centre-test-tag {
    font-size: 11px;
    padding: 3px 8px;
    border-radius: 6px;
    background: var(--primary-soft);
    color: var(--primary);
    font-weight: 700;
    white-space: nowrap;
}
.centre-body { font-size: 12.5px; color: var(--text-3); line-height: 1.7; }
.centre-body strong { color: var(--text); font-weight: 600; }
.util-bar {
    height: 6px;
    background: #F1F5F9;
    border-radius: 999px;
    overflow: hidden;
    margin-top: 6px;
}
.util-fill {
    height: 100%;
    background: linear-gradient(90deg, var(--primary) 0%, #3B82F6 100%);
    border-radius: 999px;
    transition: width 0.4s ease;
}

.patient-list { display: flex; flex-direction: column; gap: 12px; }
.patient-card {
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 14px 16px;
    background: #fff;
}
.patient-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    margin-bottom: 10px;
}
.patient-id { font-weight: 700; font-size: 13px; color: var(--text); }

.sequence-list { display: flex; flex-direction: column; gap: 2px; margin-top: 6px; }
.sequence-step {
    display: flex;
    align-items: center;
    gap: 10px;
    font-size: 12px;
    color: var(--text-2);
    padding: 6px 0;
    border-bottom: 1px solid var(--border-2);
}
.sequence-step:last-child { border-bottom: none; }
.step-num {
    width: 22px; height: 22px;
    border-radius: 50%;
    background: var(--primary-soft);
    color: var(--primary);
    display: inline-flex;
    align-items: center;
    justify-content: center;
    font-weight: 700;
    font-size: 10.5px;
    flex-shrink: 0;
}
.step-test { font-weight: 600; color: var(--text); }
.step-time { color: var(--text-4); margin-left: auto; font-family: ui-monospace, monospace; font-size: 11px; }

.timeline { display: flex; flex-direction: column; gap: 4px; max-height: 420px; overflow-y: auto; }
.timeline-row {
    display: flex;
    gap: 12px;
    align-items: flex-start;
    font-size: 12.5px;
    padding: 7px 0;
    border-bottom: 1px solid var(--border-2);
}
.timeline-row:last-child { border-bottom: none; }
.timeline-time {
    font-family: ui-monospace, monospace;
    font-size: 11.5px;
    color: var(--text-3);
    min-width: 56px;
    padding-top: 1px;
}
.timeline-event { flex: 1; color: var(--text-2); line-height: 1.5; }
.timeline-event strong { color: var(--text); font-weight: 600; }

/* Responsive */
@media (max-width: 1024px) {
    .stats-grid   { grid-template-columns: repeat(2, 1fr); }
    .actions-grid { grid-template-columns: repeat(2, 1fr); }
    .mini-stats   { grid-template-columns: repeat(2, 1fr); }
}

@media (max-width: 768px) {
    .sidebar { transform: translateX(-100%); box-shadow: var(--shadow-lg); }
    .sidebar.open { transform: none; }
    .main { margin-left: 0; }
    .menu-btn { display: inline-flex; }
    .topbar { padding: 12px 16px; gap: 12px; }
    .topbar-search { display: none; }
    .topbar-date { display: none; }
    .content { padding: 16px; }
    .welcome { padding: 20px; }
    .welcome-title { font-size: 20px; }
    .form-grid { grid-template-columns: 1fr; }
    .sidebar-overlay {
        display: none;
        position: fixed;
        inset: 0;
        background: rgba(15,23,42,0.42);
        z-index: 35;
        backdrop-filter: blur(2px);
    }
    .sidebar-overlay.open { display: block; }
    .toast { left: 16px; right: 16px; bottom: 16px; max-width: none; }
}

@media (max-width: 520px) {
    .stats-grid   { grid-template-columns: 1fr; }
    .actions-grid { grid-template-columns: 1fr; }
    .mini-stats   { grid-template-columns: 1fr; }
    .topbar-title h1 { font-size: 14px; }
    .topbar-sub { font-size: 11px; }
    .welcome-actions .btn { flex: 1; justify-content: center; }
}
</style>
</head>
<body>
<div class="app">

    <!-- ============================================================
         SIDEBAR
         ============================================================ -->
    <aside class="sidebar" id="sidebar" aria-label="Main navigation">
        <div class="sidebar-brand">
            <div class="brand-mark">P</div>
            <div class="brand-text">
                <span class="brand-name">PRATIKSHA</span>
                <span class="brand-sub">Hospital</span>
            </div>
        </div>

        <nav class="sidebar-nav">
            <div class="nav-section-label">Main</div>

            <button class="nav-item active" data-page="dashboard">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/></svg>
                <span>Overview</span>
            </button>

            <button class="nav-item" data-page="patients">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>
                <span>Patients</span>
            </button>

            <button class="nav-item" data-page="doctors">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4.8 2.3A.3.3 0 1 0 5 2H4a2 2 0 0 0-2 2v5a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6V4a2 2 0 0 0-2-2h-1a.2.2 0 1 0 .3.3"/><path d="M8 15v1a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6v-4"/><circle cx="20" cy="10" r="2"/></svg>
                <span>Doctors</span>
            </button>

            <button class="nav-item" data-page="appointments">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>
                <span>Appointments</span>
            </button>

            <button class="nav-item" data-page="tests">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10 2v7.527a2 2 0 0 1-.211.896L4.72 20.55a1 1 0 0 0 .9 1.45h12.76a1 1 0 0 0 .9-1.45l-5.069-10.127A2 2 0 0 1 14 9.527V2"/><path d="M8.5 2h7"/><path d="M7 16h10"/></svg>
                <span>Medical Tests</span>
            </button>

            <button class="nav-item" data-page="queue">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="10" y1="6" x2="21" y2="6"/><line x1="10" y1="12" x2="21" y2="12"/><line x1="10" y1="18" x2="21" y2="18"/><path d="M4 6h1v4"/><path d="M4 10h2"/><path d="M6 18H4c0-1 2-2 2-3s-1-1.5-2-1"/></svg>
                <span>Diagnostic Queue</span>
            </button>

            <button class="nav-item" data-page="billing">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="5" width="20" height="14" rx="2"/><line x1="2" y1="10" x2="22" y2="10"/></svg>
                <span>Billing</span>
            </button>

            <button class="nav-item" data-page="reports">
                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="20" x2="12" y2="10"/><line x1="18" y1="20" x2="18" y2="4"/><line x1="6" y1="20" x2="6" y2="16"/></svg>
                <span>Reports</span>
            </button>
        </nav>

        <div class="sidebar-status">
            <div class="status-head">
                <span class="status-label">System Status</span>
                <span class="status-pill pending" id="sidebarStatusPill">
                    <span class="dot-live"></span>Connecting
                </span>
            </div>
            <div class="status-detail" id="sidebarStatusDetail">Checking backend...</div>
        </div>
    </aside>

    <div class="sidebar-overlay" id="sidebarOverlay"></div>

    <!-- ============================================================
         MAIN
         ============================================================ -->
    <div class="main">

        <!-- Topbar -->
        <header class="topbar">
            <button class="icon-btn menu-btn" id="menuBtn" aria-label="Open menu">
                <svg class="icon" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="18" x2="21" y2="18"/></svg>
            </button>

            <div class="topbar-title">
                <h1 id="pageTitle">Overview</h1>
                <span class="topbar-sub" id="pageSub">Dashboard</span>
            </div>

            <div class="topbar-search">
                <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                <input id="topbarSearch" type="text" placeholder="Search medical tests..." autocomplete="off" />
            </div>

            <div class="topbar-actions">
                <span class="topbar-date" id="topbarDate"></span>
                <button class="icon-btn" aria-label="Notifications">
                    <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/></svg>
                    <span class="dot"></span>
                </button>
                <div class="avatar" title="Babu Jha">BK</div>
            </div>
        </header>

        <!-- Content -->
        <main class="content">

            <!-- ====================================================
                 PAGE: DASHBOARD
                 ==================================================== -->
            <section class="page active" id="page-dashboard">

                <div class="welcome">
                    <div class="welcome-text">
                        <div class="welcome-eyebrow" id="greeting">Good Afternoon</div>
                        <h2 class="welcome-title">Welcome to Pratiksha Hospital Management System</h2>
                        <p class="welcome-sub">Manage hospital operations from one place.</p>
                    </div>
                    <div class="welcome-actions">
                        <button class="btn btn-primary" data-goto="patients">
                            <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
                            Register Patient
                        </button>
                        <button class="btn btn-secondary" data-goto="appointments">
                            <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>
                            Book Appointment
                        </button>
                    </div>
                </div>

                <div class="stats-grid" id="statsGrid"><!-- JS --></div>

                <div class="section">
                    <div class="section-head">
                        <div><div class="section-title">Quick Actions</div></div>
                    </div>
                    <div class="actions-grid">
                        <button class="action-card" data-goto="patients">
                            <div class="action-icon">
                                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><line x1="19" y1="8" x2="19" y2="14"/><line x1="22" y1="11" x2="16" y2="11"/></svg>
                            </div>
                            <div class="action-title">Register New Patient</div>
                            <div class="action-desc">Add a new patient record to the system</div>
                        </button>
                        <button class="action-card" data-goto="appointments">
                            <div class="action-icon">
                                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/><line x1="12" y1="14" x2="12" y2="18"/><line x1="10" y1="16" x2="14" y2="16"/></svg>
                            </div>
                            <div class="action-title">Schedule Appointment</div>
                            <div class="action-desc">Book a consultation with a doctor</div>
                        </button>
                        <button class="action-card" data-goto="tests">
                            <div class="action-icon">
                                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10 2v7.527a2 2 0 0 1-.211.896L4.72 20.55a1 1 0 0 0 .9 1.45h12.76a1 1 0 0 0 .9-1.45l-5.069-10.127A2 2 0 0 1 14 9.527V2"/><path d="M8.5 2h7"/><path d="M7 16h10"/></svg>
                            </div>
                            <div class="action-title">View Medical Tests</div>
                            <div class="action-desc">Browse available diagnostic tests</div>
                        </button>
                        <button class="action-card" data-goto="reports">
                            <div class="action-icon">
                                <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>
                            </div>
                            <div class="action-title">Generate Report</div>
                            <div class="action-desc">View analytics and reports</div>
                        </button>
                    </div>
                </div>

                <div class="section">
                    <div class="section-head">
                        <div>
                            <div class="section-title">Medical Tests Overview</div>
                            <div class="section-sub">Available diagnostic tests from backend</div>
                        </div>
                        <button class="btn btn-ghost" data-goto="tests">View all →</button>
                    </div>
                    <div class="card">
                        <div id="dashboardTests"><!-- JS --></div>
                    </div>
                </div>

                <div class="section">
                    <div class="section-head">
                        <div><div class="section-title">System Health</div></div>
                    </div>
                    <div class="card">
                        <div class="card-body">
                            <div class="kv-list" id="systemHealth"><!-- JS --></div>
                        </div>
                    </div>
                </div>

            </section>

            <!-- ====================================================
                 PAGE: MEDICAL TESTS
                 ==================================================== -->
            <section class="page" id="page-tests">
                <div class="page-header">
                    <div>
                        <h2>Medical Tests</h2>
                        <p>Manage available diagnostic tests.</p>
                    </div>
                </div>

                <div class="toolbar">
                    <div class="search-box">
                        <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                        <input id="testsSearch" type="text" placeholder="Search tests by name or ID..." autocomplete="off" />
                    </div>
                    <button class="btn btn-secondary" id="testsRefresh">
                        <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 12a9 9 0 0 1 15-6.7L21 8"/><path d="M21 3v5h-5"/><path d="M21 12a9 9 0 0 1-15 6.7L3 16"/><path d="M3 21v-5h5"/></svg>
                        Refresh
                    </button>
                </div>

                <div class="card">
                    <div id="testsContent"><!-- JS --></div>
                </div>
            </section>

            <!-- ====================================================
                 PAGE: DIAGNOSTIC QUEUE (NEW)
                 ==================================================== -->
            <section class="page" id="page-queue">
                <div class="page-header">
                    <div>
                        <h2>Diagnostic Queue</h2>
                        <p>Smart patient test routing and queue management.</p>
                    </div>
                    <div class="page-header-actions">
                        <button class="btn btn-secondary" id="queueClearBtn">Clear All</button>
                        <button class="btn btn-primary" id="queueComputeBtn">
                            <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 12a9 9 0 0 1 15-6.7L21 8"/><path d="M21 3v5h-5"/><path d="M21 12a9 9 0 0 1-15 6.7L3 16"/><path d="M3 21v-5h5"/></svg>
                            Compute Queue
                        </button>
                    </div>
                </div>

                <div class="form-note" style="margin-top:0; margin-bottom:20px;">
                    <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
                    <span>Frontend prototype. Patients are stored in browser memory only. Real patient data and C++ algorithm integration will follow in the next step.</span>
                </div>

                <div class="queue-grid">

                    <!-- LEFT: Entry + Patient list -->
                    <div>
                        <div class="card">
                            <div class="card-body">
                                <div class="section-title" style="margin-bottom:14px;">Patient Test Entry</div>

                                <div class="form-field" style="margin-bottom:14px;">
                                    <label class="form-label" for="qPatientId">Patient ID</label>
                                    <input class="form-input" id="qPatientId" type="text" placeholder="e.g. P001" autocomplete="off" />
                                </div>

                                <div class="form-field" style="margin-bottom:14px;">
                                    <label class="form-label">Prescribed Tests</label>
                                    <div class="test-picker" id="qTestPicker"><!-- JS --></div>
                                </div>

                                <button class="btn btn-primary" id="qAddPatientBtn" style="width:100%; justify-content:center;">
                                    <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
                                    Add Patient to Queue
                                </button>
                            </div>
                        </div>

                        <div class="card" style="margin-top:20px;">
                            <div class="card-body">
                                <div class="section-head" style="margin-bottom:12px;">
                                    <div class="section-title">Patients in Queue</div>
                                    <span class="badge badge-grey" id="qPatientCount">0</span>
                                </div>
                                <div id="qPatientList" class="patient-list"><!-- JS --></div>
                            </div>
                        </div>
                    </div>

                    <!-- RIGHT: Results -->
                    <div>
                        <div class="card" style="margin-bottom:20px;">
                            <div class="card-body">
                                <div class="section-title" style="margin-bottom:14px;">Testing Centres</div>
                                <div class="centre-grid" id="qCentres"><!-- JS --></div>
                            </div>
                        </div>

                        <div class="card" style="margin-bottom:20px;">
                            <div class="card-body">
                                <div class="section-title" style="margin-bottom:14px;">Queue Sequence per Patient</div>
                                <div class="patient-list" id="qSequence"><!-- JS --></div>
                            </div>
                        </div>

                        <div class="card">
                            <div class="card-body">
                                <div class="section-title" style="margin-bottom:14px;">Schedule Timeline</div>
                                <div class="timeline" id="qTimeline"><!-- JS --></div>
                            </div>
                        </div>
                    </div>

                </div>
            </section>

            <!-- ====================================================
                 PAGE: PATIENTS (unchanged)
                 ==================================================== -->
            <section class="page" id="page-patients">
                <div class="page-header">
                    <div>
                        <h2>Patients</h2>
                        <p>Manage patient records.</p>
                    </div>
                    <div class="page-header-actions">
                        <button class="btn btn-primary" id="togglePatientForm">
                            <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
                            Add Patient
                        </button>
                    </div>
                </div>

                <div class="form-card" id="patientForm">
                    <div class="form-grid">
                        <div class="form-field full">
                            <label class="form-label" for="patName">Patient Name</label>
                            <input class="form-input" id="patName" type="text" placeholder="Full name" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patAge">Age</label>
                            <input class="form-input" id="patAge" type="number" min="0" placeholder="e.g. 34" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patGender">Gender</label>
                            <select class="form-select" id="patGender">
                                <option value="">Select gender</option>
                                <option>Male</option>
                                <option>Female</option>
                                <option>Other</option>
                            </select>
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patPhone">Phone Number</label>
                            <input class="form-input" id="patPhone" type="tel" placeholder="+91 ..." />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patEmail">Email</label>
                            <input class="form-input" id="patEmail" type="email" placeholder="name@example.com" />
                        </div>
                        <div class="form-field full">
                            <label class="form-label" for="patAddress">Address</label>
                            <textarea class="form-textarea" id="patAddress" placeholder="Residential address"></textarea>
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patBlood">Blood Group</label>
                            <select class="form-select" id="patBlood">
                                <option value="">Select blood group</option>
                                <option>A+</option><option>A-</option>
                                <option>B+</option><option>B-</option>
                                <option>AB+</option><option>AB-</option>
                                <option>O+</option><option>O-</option>
                            </select>
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="patEmergency">Emergency Contact</label>
                            <input class="form-input" id="patEmergency" type="tel" placeholder="+91 ..." />
                        </div>
                    </div>

                    <div class="form-note">
                        <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
                        <span>Patient registration API is not connected yet. The form is UI-only until backend support is added.</span>
                    </div>

                    <div class="form-actions">
                        <button class="btn btn-secondary" data-close-form="patientForm">Cancel</button>
                        <button class="btn btn-primary" id="savePatientBtn">Save Patient</button>
                    </div>
                </div>

                <div class="toolbar">
                    <div class="search-box">
                        <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                        <input type="text" placeholder="Search patient..." />
                    </div>
                </div>

                <div class="card">
                    <div class="table-wrap">
                        <table class="data-table">
                            <thead>
                                <tr>
                                    <th>Patient ID</th>
                                    <th>Name</th>
                                    <th>Age</th>
                                    <th>Contact</th>
                                    <th>Actions</th>
                                </tr>
                            </thead>
                        </table>
                    </div>
                    <div class="empty-state">
                        <div class="empty-icon">
                            <svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>
                        </div>
                        <div class="empty-title">No patient records available</div>
                        <div class="empty-desc">Patient API is not yet connected to the backend. Once implemented, patient records will appear here.</div>
                    </div>
                </div>
            </section>

            <!-- ====================================================
                 PAGE: DOCTORS (unchanged)
                 ==================================================== -->
            <section class="page" id="page-doctors">
                <div class="page-header">
                    <div>
                        <h2>Doctors</h2>
                        <p>Manage doctor records and specializations.</p>
                    </div>
                </div>

                <div class="mini-stats">
                    <div class="mini-stat">
                        <div class="mini-stat-label">Total Doctors</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                    <div class="mini-stat">
                        <div class="mini-stat-label">Available Doctors</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                    <div class="mini-stat">
                        <div class="mini-stat-label">Specializations</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                </div>

                <div class="card">
                    <div class="table-wrap">
                        <table class="data-table">
                            <thead>
                                <tr>
                                    <th>Doctor ID</th>
                                    <th>Name</th>
                                    <th>Specialization</th>
                                    <th>Contact</th>
                                    <th>Status</th>
                                </tr>
                            </thead>
                        </table>
                    </div>
                    <div class="empty-state">
                        <div class="empty-icon">
                            <svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4.8 2.3A.3.3 0 1 0 5 2H4a2 2 0 0 0-2 2v5a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6V4a2 2 0 0 0-2-2h-1a.2.2 0 1 0 .3.3"/><path d="M8 15v1a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6v-4"/><circle cx="20" cy="10" r="2"/></svg>
                        </div>
                        <div class="empty-title">No doctor records available</div>
                        <div class="empty-desc">Doctor API is not yet connected. Doctor records will appear here once backend support is added.</div>
                    </div>
                </div>
            </section>

            <!-- ====================================================
                 PAGE: APPOINTMENTS (unchanged)
                 ==================================================== -->
            <section class="page" id="page-appointments">
                <div class="page-header">
                    <div>
                        <h2>Appointments</h2>
                        <p>Manage patient appointments.</p>
                    </div>
                    <div class="page-header-actions">
                        <button class="btn btn-primary" id="toggleApptForm">
                            <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
                            New Appointment
                        </button>
                    </div>
                </div>

                <div class="form-card" id="apptForm">
                    <div class="form-grid">
                        <div class="form-field">
                            <label class="form-label" for="apptPatient">Patient</label>
                            <input class="form-input" id="apptPatient" type="text" placeholder="Patient name or ID" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="apptDoctor">Doctor</label>
                            <input class="form-input" id="apptDoctor" type="text" placeholder="Doctor name" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="apptDate">Appointment Date</label>
                            <input class="form-input" id="apptDate" type="date" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="apptTime">Appointment Time</label>
                            <input class="form-input" id="apptTime" type="time" />
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="apptDept">Department</label>
                            <select class="form-select" id="apptDept">
                                <option value="">Select department</option>
                                <option>General</option>
                                <option>Radiology</option>
                                <option>Pathology</option>
                                <option>Cardiology</option>
                                <option>Orthopedics</option>
                            </select>
                        </div>
                        <div class="form-field">
                            <label class="form-label" for="apptReason">Reason for Visit</label>
                            <input class="form-input" id="apptReason" type="text" placeholder="Brief reason" />
                        </div>
                    </div>

                    <div class="form-note">
                        <svg class="icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
                        <span>Appointment API is not connected yet. This form is UI-only until backend support is added.</span>
                    </div>

                    <div class="form-actions">
                        <button class="btn btn-secondary" data-close-form="apptForm">Cancel</button>
                        <button class="btn btn-primary" id="saveApptBtn">Save Appointment</button>
                    </div>
                </div>

                <div class="card">
                    <div class="table-wrap">
                        <table class="data-table">
                            <thead>
                                <tr>
                                    <th>Appointment ID</th>
                                    <th>Patient Name</th>
                                    <th>Doctor Name</th>
                                    <th>Date</th>
                                    <th>Time</th>
                                    <th>Status</th>
                                </tr>
                            </thead>
                        </table>
                    </div>
                    <div class="empty-state">
                        <div class="empty-icon">
                            <svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>
                        </div>
                        <div class="empty-title">No appointments scheduled</div>
                        <div class="empty-desc">Appointment API is not yet connected. Scheduled appointments will appear here.</div>
                    </div>
                </div>
            </section>

            <!-- ====================================================
                 PAGE: BILLING (unchanged)
                 ==================================================== -->
            <section class="page" id="page-billing">
                <div class="page-header">
                    <div>
                        <h2>Billing</h2>
                        <p>Invoices, consultation fees and payments.</p>
                    </div>
                </div>

                <div class="mini-stats">
                    <div class="mini-stat">
                        <div class="mini-stat-label">Total Invoices</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                    <div class="mini-stat">
                        <div class="mini-stat-label">Total Revenue</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                    <div class="mini-stat">
                        <div class="mini-stat-label">Pending Payments</div>
                        <div class="mini-stat-value">—</div>
                    </div>
                </div>

                <div class="card">
                    <div class="table-wrap">
                        <table class="data-table">
                            <thead>
                                <tr>
                                    <th>Invoice ID</th>
                                    <th>Patient</th>
                                    <th>Consultation</th>
                                    <th>Test Charges</th>
                                    <th>Total</th>
                                    <th>Status</th>
                                </tr>
                            </thead>
                        </table>
                    </div>
                    <div class="empty-state">
                        <div class="empty-icon">
                            <svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="5" width="20" height="14" rx="2"/><line x1="2" y1="10" x2="22" y2="10"/></svg>
                        </div>
                        <div class="empty-title">No invoices available</div>
                        <div class="empty-desc">Billing API is not yet connected. Invoices will appear here once backend support is added.</div>
                    </div>
                </div>
            </section>

            <!-- ====================================================
                 PAGE: REPORTS (unchanged)
                 ==================================================== -->
            <section class="page" id="page-reports">
                <div class="page-header">
                    <div>
                        <h2>Reports</h2>
                        <p>Analytics and hospital insights.</p>
                    </div>
                </div>

                <div class="actions-grid" style="margin-bottom:20px;">
                    <div class="action-card" style="cursor:default;">
                        <div class="action-icon">
                            <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/></svg>
                        </div>
                        <div class="action-title">Patient Registration Report</div>
                        <div class="action-desc">New patient registrations over time</div>
                    </div>
                    <div class="action-card" style="cursor:default;">
                        <div class="action-icon">
                            <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>
                        </div>
                        <div class="action-title">Appointment Report</div>
                        <div class="action-desc">Scheduled vs completed appointments</div>
                    </div>
                    <div class="action-card" style="cursor:default;">
                        <div class="action-icon">
                            <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10 2v7.527a2 2 0 0 1-.211.896L4.72 20.55a1 1 0 0 0 .9 1.45h12.76a1 1 0 0 0 .9-1.45l-5.069-10.127A2 2 0 0 1 14 9.527V2"/><path d="M8.5 2h7"/><path d="M7 16h10"/></svg>
                        </div>
                        <div class="action-title">Medical Tests Report</div>
                        <div class="action-desc">Most performed diagnostic tests</div>
                    </div>
                    <div class="action-card" style="cursor:default;">
                        <div class="action-icon">
                            <svg class="icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="20" x2="12" y2="10"/><line x1="18" y1="20" x2="18" y2="4"/><line x1="6" y1="20" x2="6" y2="16"/></svg>
                        </div>
                        <div class="action-title">Revenue Report</div>
                        <div class="action-desc">Revenue breakdown by department</div>
                    </div>
                </div>

                <div class="card">
                    <div class="empty-state">
                        <div class="empty-icon">
                            <svg class="icon" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="12" y1="20" x2="12" y2="10"/><line x1="18" y1="20" x2="18" y2="4"/><line x1="6" y1="20" x2="6" y2="16"/></svg>
                        </div>
                        <div class="empty-title">No report data available</div>
                        <div class="empty-desc">Reports will populate once patient, appointment and billing APIs are connected to the backend.</div>
                    </div>
                </div>
            </section>

        </main>
    </div>
</div>

<!-- ============================================================
     JAVASCRIPT
     ============================================================ -->
<script>
(function () {
    'use strict';

    /* --------------------------------------------------------
       Icon paths
       -------------------------------------------------------- */
    const ICON_PATHS = {
        'users':        '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>',
        'stethoscope':  '<path d="M4.8 2.3A.3.3 0 1 0 5 2H4a2 2 0 0 0-2 2v5a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6V4a2 2 0 0 0-2-2h-1a.2.2 0 1 0 .3.3"/><path d="M8 15v1a6 6 0 0 0 6 6v0a6 6 0 0 0 6-6v-4"/><circle cx="20" cy="10" r="2"/>',
        'calendar':     '<rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/>',
        'flask':        '<path d="M10 2v7.527a2 2 0 0 1-.211.896L4.72 20.55a1 1 0 0 0 .9 1.45h12.76a1 1 0 0 0 .9-1.45l-5.069-10.127A2 2 0 0 1 14 9.527V2"/><path d="M8.5 2h7"/><path d="M7 16h10"/>',
        'search':       '<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>',
        'refresh':      '<path d="M3 12a9 9 0 0 1 15-6.7L21 8"/><path d="M21 3v5h-5"/><path d="M21 12a9 9 0 0 1-15 6.7L3 16"/><path d="M3 21v-5h5"/>',
        'check':        '<polyline points="20 6 9 17 4 12"/>',
        'alert':        '<circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/>'
    };

    function svgIcon(name, size) {
        size = size || 20;
        var inner = ICON_PATHS[name] || '';
        return '<svg class="icon" width="' + size + '" height="' + size + '" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + inner + '</svg>';
    }

    /* --------------------------------------------------------
       State
       -------------------------------------------------------- */
    var state = {
        page: 'dashboard',
        health: null,
        backendOnline: null,
        tests: [],
        testsLoading: false,
        testsError: null,
        testSearch: ''
    };

    /* Queue state */
    var QUEUE_TESTS = {
        A: { name: 'X-Ray',      duration: 10 },
        B: { name: 'Blood Test', duration: 5  },
        C: { name: 'Ultrasound', duration: 20 },
        D: { name: 'CT Scan',    duration: 30 }
    };

    var queueState = {
        patients: [],        // { id, tests: ['A','B'], addedAt: number }
        lastResult: null,
        pickerRendered: false
    };

    /* --------------------------------------------------------
       API helpers
       -------------------------------------------------------- */
    function apiGet(path) {
        return fetch(path, { cache: 'no-store' }).then(function (res) {
            if (!res.ok) throw new Error('HTTP ' + res.status);
            return res.json();
        });
    }

    function escapeHtml(value) {
        return String(value == null ? '' : value)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    /* --------------------------------------------------------
       Toast
       -------------------------------------------------------- */
    var toastTimer = null;
    function showToast(message, type) {
        var existing = document.querySelector('.toast');
        if (existing) existing.remove();
        var el = document.createElement('div');
        el.className = 'toast' + (type ? ' ' + type : '');
        el.innerHTML = svgIcon(type === 'error' ? 'alert' : 'check', 16) + '<span>' + escapeHtml(message) + '</span>';
        document.body.appendChild(el);
        if (toastTimer) clearTimeout(toastTimer);
        toastTimer = setTimeout(function () { el.remove(); }, 4200);
    }

    /* --------------------------------------------------------
       Health / status
       -------------------------------------------------------- */
    function loadHealth() {
        return apiGet('/api/health').then(function (data) {
            state.health = data;
            state.backendOnline = true;
            renderHealth();
        }).catch(function () {
            state.health = null;
            state.backendOnline = false;
            renderHealth();
        });
    }

    function renderHealth() {
        var pill = document.getElementById('sidebarStatusPill');
        var detail = document.getElementById('sidebarStatusDetail');

        if (state.backendOnline === null) {
            pill.className = 'status-pill pending';
            pill.innerHTML = '<span class="dot-live"></span>Connecting';
            detail.textContent = 'Checking backend...';
        } else if (state.backendOnline) {
            pill.className = 'status-pill';
            pill.innerHTML = '<span class="dot-live"></span>Connected';
            detail.textContent = 'Backend is running';
        } else {
            pill.className = 'status-pill disconnected';
            pill.innerHTML = '<span class="dot-live"></span>Disconnected';
            detail.textContent = 'Backend is offline';
        }

        renderSystemHealth();
        renderStats();
    }

    function renderSystemHealth() {
        var el = document.getElementById('systemHealth');
        if (!el) return;

        var online = state.backendOnline === true;
        var h = state.health || {};

        function row(key, valueHtml) {
            return '<div class="kv-row"><span class="kv-key">' + key + '</span><span class="kv-val">' + valueHtml + '</span></div>';
        }

        var backendBadge = online
            ? '<span class="badge badge-green">' + svgIcon('check', 12) + ' Connected</span>'
            : '<span class="badge badge-grey">Disconnected</span>';

        el.innerHTML = [
            row('Backend', backendBadge),
            row('Application', online ? escapeHtml(h.application || '—') : '—'),
            row('Version', online ? escapeHtml(h.version || '—') : '—'),
            row('Backend Technology', online ? escapeHtml(h.backend || '—') : '—'),
            row('Status', online ? escapeHtml(h.status || '—') : 'Disconnected')
        ].join('');
    }

    /* --------------------------------------------------------
       Stats
       -------------------------------------------------------- */
    function renderStats() {
        var el = document.getElementById('statsGrid');
        if (!el) return;

        var testCount, testHint;
        if (state.testsLoading) {
            testCount = '…';
            testHint = '<span class="stat-hint">Loading…</span>';
        } else if (state.testsError) {
            testCount = '—';
            testHint = '<span class="stat-hint warn">Backend unavailable</span>';
        } else {
            testCount = String(state.tests.length);
            testHint = '<span class="stat-hint ok">Available tests</span>';
        }

        el.innerHTML = [
            '<div class="stat-card">',
                '<div class="stat-top"><div class="stat-icon blue">' + svgIcon('users', 20) + '</div></div>',
                '<div class="stat-value">—</div>',
                '<div class="stat-label">Total Patients</div>',
                '<div class="stat-hint">API not connected</div>',
            '</div>',
            '<div class="stat-card">',
                '<div class="stat-top"><div class="stat-icon violet">' + svgIcon('stethoscope', 20) + '</div></div>',
                '<div class="stat-value">—</div>',
                '<div class="stat-label">Total Doctors</div>',
                '<div class="stat-hint">API not connected</div>',
            '</div>',
            '<div class="stat-card">',
                '<div class="stat-top"><div class="stat-icon amber">' + svgIcon('calendar', 20) + '</div></div>',
                '<div class="stat-value">—</div>',
                '<div class="stat-label">Appointments</div>',
                '<div class="stat-hint">API not connected</div>',
            '</div>',
            '<div class="stat-card">',
                '<div class="stat-top"><div class="stat-icon teal">' + svgIcon('flask', 20) + '</div></div>',
                '<div class="stat-value">' + testCount + '</div>',
                '<div class="stat-label">Medical Tests</div>',
                testHint,
            '</div>'
        ].join('');
    }

    /* --------------------------------------------------------
       Tests page
       -------------------------------------------------------- */
    function loadTests() {
        state.testsLoading = true;
        state.testsError = null;
        renderTestsPage();
        renderDashboardTests();
        renderStats();

        return apiGet('/api/tests').then(function (data) {
            var list = (data && Array.isArray(data.tests)) ? data.tests : [];
            state.tests = list;
            state.testsLoading = false;
            state.testsError = null;
            renderTestsPage();
            renderDashboardTests();
            renderStats();
        }).catch(function (err) {
            state.tests = [];
            state.testsLoading = false;
            state.testsError = 'Unable to load medical tests. (' + err.message + ')';
            renderTestsPage();
            renderDashboardTests();
            renderStats();
        });
    }

    function filterTests() {
        var q = (state.testSearch || '').trim().toLowerCase();
        if (!q) return state.tests;
        return state.tests.filter(function (t) {
            var name = (t.name || '').toLowerCase();
            var id = (t.id || '').toLowerCase();
            return name.indexOf(q) !== -1 || id.indexOf(q) !== -1;
        });
    }

    function buildTestTable(tests) {
        var rows = tests.map(function (t) {
            return '<tr>' +
                '<td><span class="test-id">' + escapeHtml(t.id) + '</span></td>' +
                '<td class="cell-strong">' + escapeHtml(t.name) + '</td>' +
                '<td>' + escapeHtml(t.durationMinutes) + ' min</td>' +
                '<td><span class="badge badge-green">' + svgIcon('check', 12) + ' Available</span></td>' +
            '</tr>';
        }).join('');

        return '<div class="table-wrap"><table class="data-table">' +
            '<thead><tr><th>ID</th><th>Test Name</th><th>Duration</th><th>Status</th></tr></thead>' +
            '<tbody>' + rows + '</tbody>' +
        '</table></div>';
    }

    function renderTestsPage() {
        var el = document.getElementById('testsContent');
        if (!el) return;

        if (state.testsLoading) {
            el.innerHTML = '<div class="loading-state"><div class="spinner"></div>Loading medical tests...</div>';
            return;
        }
        if (state.testsError) {
            el.innerHTML =
                '<div class="error-state">' +
                    '<div class="error-icon">' + svgIcon('alert', 22) + '</div>' +
                    '<div class="error-title">Unable to load medical tests</div>' +
                    '<div class="error-desc">' + escapeHtml(state.testsError) + '</div>' +
                    '<button class="btn btn-secondary" id="testsRetryBtn">' + svgIcon('refresh', 16) + ' Retry</button>' +
                '</div>';
            var retry = document.getElementById('testsRetryBtn');
            if (retry) retry.addEventListener('click', loadTests);
            return;
        }
        if (state.tests.length === 0) {
            el.innerHTML =
                '<div class="empty-state">' +
                    '<div class="empty-icon">' + svgIcon('flask', 22) + '</div>' +
                    '<div class="empty-title">No medical tests found</div>' +
                    '<div class="empty-desc">The backend returned no diagnostic tests.</div>' +
                '</div>';
            return;
        }
        var filtered = filterTests();
        if (filtered.length === 0) {
            el.innerHTML =
                '<div class="empty-state">' +
                    '<div class="empty-icon">' + svgIcon('search', 22) + '</div>' +
                    '<div class="empty-title">No matching tests</div>' +
                    '<div class="empty-desc">Try a different search term or clear the search box.</div>' +
                '</div>';
            return;
        }
        el.innerHTML = buildTestTable(filtered);
    }

    function renderDashboardTests() {
        var el = document.getElementById('dashboardTests');
        if (!el) return;

        if (state.testsLoading) {
            el.innerHTML = '<div class="loading-state"><div class="spinner"></div>Loading...</div>';
            return;
        }
        if (state.testsError) {
            el.innerHTML =
                '<div class="error-state" style="padding:28px 20px;">' +
                    '<div class="error-title">Unable to load medical tests</div>' +
                    '<div class="error-desc" style="margin-bottom:0;">' + escapeHtml(state.testsError) + '</div>' +
                '</div>';
            return;
        }
        if (state.tests.length === 0) {
            el.innerHTML = '<div class="empty-state" style="padding:32px 20px;"><div class="empty-title">No tests available</div></div>';
            return;
        }
        el.innerHTML = buildTestTable(state.tests);
    }

    /* ========================================================
       DIAGNOSTIC QUEUE
       ======================================================== */

    function renderQueueTestPicker() {
        if (queueState.pickerRendered) return;
        var el = document.getElementById('qTestPicker');
        if (!el) return;

        el.innerHTML = Object.keys(QUEUE_TESTS).map(function (id) {
            var t = QUEUE_TESTS[id];
            return (
                '<label class="test-pick" data-test-id="' + id + '">' +
                    '<input type="checkbox" value="' + id + '" />' +
                    '<span class="test-id">' + id + '</span>' +
                    '<span style="flex:1; font-weight:500;">' + escapeHtml(t.name) + '</span>' +
                    '<span style="font-size:11.5px; color:var(--text-3);">' + t.duration + ' min</span>' +
                '</label>'
            );
        }).join('');

        // Toggle "checked" class on parent label
        el.querySelectorAll('input[type="checkbox"]').forEach(function (cb) {
            cb.addEventListener('change', function () {
                var label = cb.closest('.test-pick');
                if (label) label.classList.toggle('checked', cb.checked);
            });
        });

        queueState.pickerRendered = true;
    }

    function renderQueuePatients() {
        var listEl = document.getElementById('qPatientList');
        var countEl = document.getElementById('qPatientCount');
        if (!listEl) return;

        if (countEl) countEl.textContent = String(queueState.patients.length);

        if (queueState.patients.length === 0) {
            listEl.innerHTML = '<div style="text-align:center; color:var(--text-3); font-size:12.5px; padding:14px 0;">No patients in queue. Add one above.</div>';
            return;
        }

        listEl.innerHTML = queueState.patients.map(function (p) {
            return (
                '<div class="patient-card">' +
                    '<div class="patient-head">' +
                        '<span class="patient-id">' + escapeHtml(p.id) + '</span>' +
                        '<button class="btn btn-ghost" data-remove-patient="' + escapeHtml(p.id) + '" ' +
                            'style="color:var(--red); padding:2px 8px; font-size:12px;">Remove</button>' +
                    '</div>' +
                    '<div style="display:flex; gap:6px; flex-wrap:wrap;">' +
                        p.tests.map(function (t) {
                            return '<span class="badge badge-grey">' + t + ' · ' + escapeHtml(QUEUE_TESTS[t].name) + '</span>';
                        }).join('') +
                    '</div>' +
                '</div>'
            );
        }).join('');
    }

    function renderQueueEmptyPanels() {
        var centresEl = document.getElementById('qCentres');
        var seqEl = document.getElementById('qSequence');
        var tlEl = document.getElementById('qTimeline');

        if (centresEl) {
            centresEl.innerHTML = Object.keys(QUEUE_TESTS).map(function (id) {
                var t = QUEUE_TESTS[id];
                return (
                    '<div class="centre-card">' +
                        '<div class="centre-head">' +
                            '<div>' +
                                '<div class="centre-name">' + escapeHtml(t.name) + '</div>' +
                                '<div class="centre-sub">Centre ' + id + '</div>' +
                            '</div>' +
                            '<span class="centre-test-tag">' + t.duration + ' min</span>' +
                        '</div>' +
                        '<div class="centre-body">Waiting for queue computation…</div>' +
                    '</div>'
                );
            }).join('');
        }
        if (seqEl) {
            seqEl.innerHTML = '<div style="text-align:center; color:var(--text-3); font-size:12.5px; padding:14px 0;">Compute the queue to see each patient’s test sequence.</div>';
        }
        if (tlEl) {
            tlEl.innerHTML = '<div style="text-align:center; color:var(--text-3); font-size:12.5px; padding:14px 0;">No events yet.</div>';
        }
    }

    function addPatientToQueue() {
        var idInput = document.getElementById('qPatientId');
        if (!idInput) return;
        var id = idInput.value.trim();

        if (!id) {
            showToast('Patient ID is required.', 'warn');
            return;
        }
        if (queueState.patients.some(function (p) { return p.id === id; })) {
            showToast('Patient ID already in queue.', 'warn');
            return;
        }
        var checks = document.querySelectorAll('#qTestPicker input[type="checkbox"]:checked');
        var tests = Array.prototype.map.call(checks, function (c) { return c.value; });

        if (tests.length === 0) {
            showToast('Select at least one test.', 'warn');
            return;
        }

        queueState.patients.push({
            id: id,
            tests: tests,
            addedAt: queueState.patients.length
        });

        idInput.value = '';
        Array.prototype.forEach.call(checks, function (c) {
            c.checked = false;
            var label = c.closest('.test-pick');
            if (label) label.classList.remove('checked');
        });

        renderQueuePatients();

        if (queueState.lastResult) {
            computeQueue(false);  // silent recompute
        } else {
            renderQueueEmptyPanels();
        }

        showToast('Patient ' + id + ' added to queue.');
    }

    function removePatientFromQueue(patientId) {
        queueState.patients = queueState.patients.filter(function (p) { return p.id !== patientId; });
        // Re-index addedAt to keep ordering stable
        queueState.patients.forEach(function (p, i) { p.addedAt = i; });

        renderQueuePatients();

        if (queueState.patients.length === 0) {
            queueState.lastResult = null;
            renderQueueEmptyPanels();
        } else if (queueState.lastResult) {
            computeQueue(false);
        }
    }

    function clearQueue() {
        queueState.patients = [];
        queueState.lastResult = null;

        var idInput = document.getElementById('qPatientId');
        if (idInput) idInput.value = '';

        var checks = document.querySelectorAll('#qTestPicker input[type="checkbox"]');
        Array.prototype.forEach.call(checks, function (c) {
            c.checked = false;
            var label = c.closest('.test-pick');
            if (label) label.classList.remove('checked');
        });

        renderQueuePatients();
        renderQueueEmptyPanels();
        showToast('Queue cleared.');
    }

    /**
     * Smart diagnostic queue algorithm (client-side prototype).
     *
     * Report rules implemented:
     *   1. First pass — distribute patients to available centres.
     *   2. Queue for common tests forms only after each relevant centre
     *      has received its first patient.
     *   3. Priority = waiting minutes + test duration (longer waiting /
     *      longer test = higher priority).
     *   4. Recalculate after each test completes.
     */
    function computeQueue(showToastOnSuccess) {
        if (queueState.patients.length === 0) {
            showToast('Add at least one patient first.', 'warn');
            return;
        }

        // Snapshot patient working copies
        var patients = queueState.patients.map(function (p) {
            return {
                id: p.id,
                remaining: p.tests.slice(),
                done: [],
                addedAt: p.addedAt
            };
        });

        // Snapshot centres — one centre per test type
        var centres = {};
        Object.keys(QUEUE_TESTS).forEach(function (id) {
            centres[id] = {
                id: id,
                name: QUEUE_TESTS[id].name,
                duration: QUEUE_TESTS[id].duration,
                current: null,
                remainingMin: 0,
                busyMinutes: 0
            };
        });

        var events = [];
        var perPatient = {};
        patients.forEach(function (p) { perPatient[p.id] = []; });

        var time = 0;
        var MAX_TIME = 5000;

        while (time < MAX_TIME) {
            // 1) Process completions
            Object.keys(centres).forEach(function (cid) {
                var c = centres[cid];
                if (c.current && c.remainingMin <= 0) {
                    var p = patients.find(function (x) { return x.id === c.current; });
                    if (p) {
                        var idx = p.remaining.indexOf(cid);
                        if (idx >= 0) p.remaining.splice(idx, 1);
                        p.done.push(cid);
                    }
                    events.push({ time: time, type: 'complete', centre: cid, patient: c.current });
                    c.current = null;
                }
            });

            // All done?
            var allDone = patients.every(function (p) { return p.remaining.length === 0; });
            if (allDone) break;

            // 2) Assign free centres (first-pass round-robin happens
            //    naturally in the very first iteration).
            var busyPatients = {};
            Object.keys(centres).forEach(function (cid) {
                if (centres[cid].current) busyPatients[centres[cid].current] = true;
            });

            Object.keys(centres).forEach(function (cid) {
                var c = centres[cid];
                if (c.current) return;

                var eligible = patients.filter(function (p) {
                    return p.remaining.indexOf(cid) !== -1 && !busyPatients[p.id];
                });
                if (eligible.length === 0) return;

                // Priority: waiting minutes + test duration
                eligible.sort(function (a, b) {
                    var waitA = Math.max(0, time - a.addedAt);
                    var waitB = Math.max(0, time - b.addedAt);
                    var scoreA = waitA + c.duration;
                    var scoreB = waitB + c.duration;
                    return scoreB - scoreA;
                });

                var chosen = eligible[0];
                c.current = chosen.id;
                c.remainingMin = c.duration;
                busyPatients[chosen.id] = true;

                events.push({ time: time, type: 'start', centre: cid, patient: chosen.id });
                perPatient[chosen.id].push({
                    test: cid,
                    startTime: time,
                    duration: c.duration
                });
            });

            // 3) Advance time
            time += 1;
            Object.keys(centres).forEach(function (cid) {
                if (centres[cid].remainingMin > 0) {
                    centres[cid].remainingMin -= 1;
                    centres[cid].busyMinutes += 1;
                }
            });
        }

        queueState.lastResult = {
            patients: patients,
            centres: centres,
            events: events,
            perPatient: perPatient,
            totalTime: time
        };

        renderQueueResult();

        if (showToastOnSuccess !== false) {
            showToast('Queue computed successfully.');
        }
    }

    function renderQueueResult() {
        var result = queueState.lastResult;
        if (!result) {
            renderQueueEmptyPanels();
            return;
        }

        /* ---- Centres ---- */
        var centresEl = document.getElementById('qCentres');
        if (centresEl) {
            centresEl.innerHTML = Object.keys(result.centres).map(function (cid) {
                var c = result.centres[cid];
                var completed = result.events.filter(function (e) {
                    return e.centre === cid && e.type === 'complete';
                }).length;
                var util = result.totalTime > 0
                    ? Math.round((c.busyMinutes / result.totalTime) * 100)
                    : 0;

                return (
                    '<div class="centre-card">' +
                        '<div class="centre-head">' +
                            '<div>' +
                                '<div class="centre-name">' + escapeHtml(c.name) + '</div>' +
                                '<div class="centre-sub">Centre ' + cid + '</div>' +
                            '</div>' +
                            '<span class="centre-test-tag">' + c.duration + ' min</span>' +
                        '</div>' +
                        '<div class="centre-body">' +
                            'Tests completed: <strong>' + completed + '</strong><br>' +
                            'Utilization: <strong>' + util + '%</strong>' +
                            '<div class="util-bar"><div class="util-fill" style="width:' + util + '%;"></div></div>' +
                        '</div>' +
                    '</div>'
                );
            }).join('');
        }

        /* ---- Per-patient sequence ---- */
        var seqEl = document.getElementById('qSequence');
        if (seqEl) {
            seqEl.innerHTML = result.patients.map(function (p) {
                var steps = result.perPatient[p.id] || [];
                if (steps.length === 0) {
                    return (
                        '<div class="patient-card">' +
                            '<div class="patient-head"><span class="patient-id">' + escapeHtml(p.id) + '</span></div>' +
                            '<div style="font-size:12.5px; color:var(--text-3);">No tests assigned.</div>' +
                        '</div>'
                    );
                }
                return (
                    '<div class="patient-card">' +
                        '<div class="patient-head">' +
                            '<span class="patient-id">' + escapeHtml(p.id) + '</span>' +
                            '<span class="badge badge-green">' + steps.length + ' test' + (steps.length > 1 ? 's' : '') + '</span>' +
                        '</div>' +
                        '<div class="sequence-list">' +
                            steps.map(function (s, i) {
                                return (
                                    '<div class="sequence-step">' +
                                        '<span class="step-num">' + (i + 1) + '</span>' +
                                        '<span class="step-test">' + s.test + ' · ' + escapeHtml(QUEUE_TESTS[s.test].name) + '</span>' +
                                        '<span class="step-time">+ ' + s.startTime + ' min</span>' +
                                    '</div>'
                                );
                            }).join('') +
                        '</div>' +
                    '</div>'
                );
            }).join('');
        }

        /* ---- Timeline ---- */
        var tlEl = document.getElementById('qTimeline');
        if (tlEl) {
            if (result.events.length === 0) {
                tlEl.innerHTML = '<div style="text-align:center; color:var(--text-3); font-size:12.5px; padding:14px 0;">No events.</div>';
            } else {
                tlEl.innerHTML = result.events.map(function (e) {
                    var isStart = e.type === 'start';
                    var color = isStart ? 'var(--primary)' : 'var(--green)';
                    var label = isStart ? 'Start' : 'Complete';
                    var icon = isStart ? '▶' : '✓';
                    return (
                        '<div class="timeline-row">' +
                            '<span class="timeline-time">' + e.time + ' min</span>' +
                            '<span class="timeline-event">' +
                                '<strong style="color:' + color + ';">' + icon + ' ' + label + '</strong>' +
                                ' · <strong>' + escapeHtml(e.patient) + '</strong>' +
                                ' · ' + escapeHtml(QUEUE_TESTS[e.centre].name) + ' (' + e.centre + ')' +
                            '</span>' +
                        '</div>'
                    );
                }).join('');
            }
        }
    }

    /* --------------------------------------------------------
       Navigation
       -------------------------------------------------------- */
    var PAGES = {
        dashboard:    { title: 'Overview',         sub: 'Dashboard' },
        patients:     { title: 'Patients',         sub: 'Patient records' },
        doctors:      { title: 'Doctors',          sub: 'Medical staff' },
        appointments: { title: 'Appointments',     sub: 'Scheduling' },
        tests:        { title: 'Medical Tests',    sub: 'Diagnostic tests' },
        queue:        { title: 'Diagnostic Queue', sub: 'Smart test routing' },
        billing:      { title: 'Billing',          sub: 'Invoices and payments' },
        reports:      { title: 'Reports',          sub: 'Analytics' }
    };

    function navigate(page) {
        if (!PAGES[page]) return;
        state.page = page;

        var sections = document.querySelectorAll('.page');
        for (var i = 0; i < sections.length; i++) {
            sections[i].classList.toggle('active', sections[i].id === 'page-' + page);
        }
        var navs = document.querySelectorAll('.nav-item');
        for (var j = 0; j < navs.length; j++) {
            navs[j].classList.toggle('active', navs[j].getAttribute('data-page') === page);
        }

        document.getElementById('pageTitle').textContent = PAGES[page].title;
        document.getElementById('pageSub').textContent = PAGES[page].sub;

        closeSidebar();
        window.scrollTo({ top: 0, behavior: 'smooth' });

        if (page === 'tests' || page === 'dashboard') {
            if (state.tests.length === 0 && !state.testsLoading && !state.testsError) {
                loadTests();
            }
            if (state.backendOnline !== true) {
                loadHealth();
            }
        }

        if (page === 'tests') {
            var testsSearch = document.getElementById('testsSearch');
            if (testsSearch && !testsSearch.value) {
                testsSearch.value = state.testSearch;
            }
        }

        if (page === 'queue') {
            renderQueueTestPicker();
            renderQueuePatients();
            if (queueState.lastResult) {
                renderQueueResult();
            } else {
                renderQueueEmptyPanels();
            }
        }
    }

    /* --------------------------------------------------------
       Sidebar (mobile)
       -------------------------------------------------------- */
    function openSidebar() {
        document.getElementById('sidebar').classList.add('open');
        document.getElementById('sidebarOverlay').classList.add('open');
    }
    function closeSidebar() {
        document.getElementById('sidebar').classList.remove('open');
        document.getElementById('sidebarOverlay').classList.remove('open');
    }

    /* --------------------------------------------------------
       Init
       -------------------------------------------------------- */
    function init() {
        // Greeting + date
        var now = new Date();
        var hour = now.getHours();
        var greeting = hour < 12 ? 'Good Morning' : (hour < 17 ? 'Good Afternoon' : 'Good Evening');
        document.getElementById('greeting').textContent = greeting;

        try {
            document.getElementById('topbarDate').textContent = now.toLocaleDateString('en-IN', {
                weekday: 'short', day: '2-digit', month: 'short', year: 'numeric'
            });
        } catch (e) {
            document.getElementById('topbarDate').textContent = '';
        }

        // Nav
        var navs = document.querySelectorAll('.nav-item');
        for (var i = 0; i < navs.length; i++) {
            navs[i].addEventListener('click', function () {
                navigate(this.getAttribute('data-page'));
            });
        }

        document.addEventListener('click', function (e) {
            var target = e.target.closest('[data-goto]');
            if (target) {
                navigate(target.getAttribute('data-goto'));
            }
        });

        // Menu
        document.getElementById('menuBtn').addEventListener('click', openSidebar);
        document.getElementById('sidebarOverlay').addEventListener('click', closeSidebar);

        // Topbar search
        var topSearch = document.getElementById('topbarSearch');
        topSearch.addEventListener('input', function () {
            if (state.page === 'tests') {
                state.testSearch = this.value;
                var ts = document.getElementById('testsSearch');
                if (ts) ts.value = this.value;
                renderTestsPage();
            }
        });
        topSearch.addEventListener('keydown', function (e) {
            if (e.key === 'Enter') {
                state.testSearch = this.value;
                var ts = document.getElementById('testsSearch');
                if (ts) ts.value = this.value;
                navigate('tests');
                renderTestsPage();
            }
        });

        // Tests page search
        var testsSearch = document.getElementById('testsSearch');
        testsSearch.addEventListener('input', function () {
            state.testSearch = this.value;
            var ts = document.getElementById('topbarSearch');
            if (ts) ts.value = this.value;
            renderTestsPage();
        });

        // Tests refresh
        document.getElementById('testsRefresh').addEventListener('click', function () {
            loadTests();
        });

        // Patient form toggle
        var patientForm = document.getElementById('patientForm');
        document.getElementById('togglePatientForm').addEventListener('click', function () {
            patientForm.classList.toggle('open');
        });

        // Appointment form toggle
        var apptForm = document.getElementById('apptForm');
        document.getElementById('toggleApptForm').addEventListener('click', function () {
            apptForm.classList.toggle('open');
        });

        // Close form buttons
        document.addEventListener('click', function (e) {
            var closer = e.target.closest('[data-close-form]');
            if (closer) {
                var id = closer.getAttribute('data-close-form');
                var form = document.getElementById(id);
                if (form) form.classList.remove('open');
            }
        });

        // Save buttons (honest — no fake success)
        document.getElementById('savePatientBtn').addEventListener('click', function () {
            showToast('Patient registration is not connected yet. Backend API is required.', 'warn');
        });
        document.getElementById('saveApptBtn').addEventListener('click', function () {
            showToast('Appointment scheduling is not connected yet. Backend API is required.', 'warn');
        });

        // Notifications button
        var notifBtn = document.querySelector('.icon-btn[aria-label="Notifications"]');
        if (notifBtn) {
            notifBtn.addEventListener('click', function () {
                showToast('No new notifications.', 'info');
            });
        }

        /* ----- Queue page wiring ----- */
        var addBtn = document.getElementById('qAddPatientBtn');
        if (addBtn) addBtn.addEventListener('click', addPatientToQueue);

        var computeBtn = document.getElementById('queueComputeBtn');
        if (computeBtn) computeBtn.addEventListener('click', function () { computeQueue(true); });

        var clearBtn = document.getElementById('queueClearBtn');
        if (clearBtn) clearBtn.addEventListener('click', clearQueue);

        var patIdInput = document.getElementById('qPatientId');
        if (patIdInput) {
            patIdInput.addEventListener('keydown', function (e) {
                if (e.key === 'Enter') addPatientToQueue();
            });
        }

        document.addEventListener('click', function (e) {
            var rem = e.target.closest('[data-remove-patient]');
            if (rem) {
                removePatientFromQueue(rem.getAttribute('data-remove-patient'));
            }
        });

        // Render queue initial state
        renderQueueTestPicker();
        renderQueuePatients();
        renderQueueEmptyPanels();

        // Initial loads
        renderStats();
        renderTestsPage();
        renderDashboardTests();
        loadHealth();
        loadTests();

        // Health polling
        setInterval(loadHealth, 20000);
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
</script>
</body>
</html>
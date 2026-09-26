#ifndef TCMG_WEBIF_JS_READERS_H_
#define TCMG_WEBIF_JS_READERS_H_

#define TCMG_READERS_JS \
	"(function () {\n" \
	"  'use strict';\n" \
	"  var body = document.getElementById('rBody');\n" \
	"  var searchInput = document.getElementById('rSearch');\n" \
	"  if (searchInput) searchInput.value = '';\n" \
	"  if (!body) return;\n" \
	"\n" \
	"  var state = { q: '', f: 'all', sort: 'label', dir: 1 };\n" \
	"  var rows = [];\n" \
	"  var timer = 0;\n" \
	"  var busy = false;\n" \
	"\n" \
	"  function $(id) { return document.getElementById(id); }\n" \
	"  function kindLabel(k) { return k === 'card' ? 'Card' : k === 'emu' ? 'EMU' : k === 'network' ? 'Network' : 'Other'; }\n" \
	"\n" \
	"  function toast(message, kind) {\n" \
	"    var box = document.querySelector('.toasts');\n" \
	"    if (!box) {\n" \
	"      box = document.createElement('div');\n" \
	"      box.className = 'toasts';\n" \
	"      box.setAttribute('role', 'status');\n" \
	"      box.setAttribute('aria-live', 'polite');\n" \
	"      document.body.appendChild(box);\n" \
	"    }\n" \
	"    var item = document.createElement('div');\n" \
	"    item.className = 'toast ' + (kind || '');\n" \
	"    item.textContent = message;\n" \
	"    box.appendChild(item);\n" \
	"    setTimeout(function () { if (item.parentNode) item.parentNode.removeChild(item); }, 2800);\n" \
	"  }\n" \
	"\n" \
	"  function matches(reader) {\n" \
	"    if (state.f === 'enabled' && !reader.enabled) return false;\n" \
	"    if (state.f === 'disabled' && reader.enabled) return false;\n" \
	"    if (state.f === 'card' && reader.kind !== 'card') return false;\n" \
	"    if (state.f === 'network' && reader.kind !== 'network') return false;\n" \
	"    if (state.f === 'emu' && reader.kind !== 'emu') return false;\n" \
	"    if (state.q) {\n" \
	"      var q = state.q.toLowerCase();\n" \
	"      var hay = [reader.label, reader.protocol, reader.device, reader.groups, reader.caid, kindLabel(reader.kind)].join(' ').toLowerCase();\n" \
	"      if (hay.indexOf(q) < 0) return false;\n" \
	"    }\n" \
	"    return true;\n" \
	"  }\n" \
	"\n" \
	"  function sortReaders(a, b) {\n" \
	"    var av = state.sort === 'enabled' ? (a.enabled ? 1 : 0) : state.sort === 'cw_ok' ? (+a.cw_ok || 0) : state.sort === 'cw_nok' ? (+a.cw_nok || 0) : String(a[state.sort] || '').toLowerCase();\n" \
	"    var bv = state.sort === 'enabled' ? (b.enabled ? 1 : 0) : state.sort === 'cw_ok' ? (+b.cw_ok || 0) : state.sort === 'cw_nok' ? (+b.cw_nok || 0) : String(b[state.sort] || '').toLowerCase();\n" \
	"    if (av === bv) return a.index - b.index;\n" \
	"    return av < bv ? -state.dir : state.dir;\n" \
	"  }\n" \
	"\n" \
	"  function updateCounts() {\n" \
	"    var enabled = 0, card = 0, network = 0, emu = 0;\n" \
	"    rows.forEach(function (reader) {\n" \
	"      if (reader.enabled) enabled++;\n" \
	"      if (reader.kind === 'card') card++;\n" \
	"      else if (reader.kind === 'network') network++;\n" \
	"      else if (reader.kind === 'emu') emu++;\n" \
	"    });\n" \
	"    var counts = { all: rows.length, enabled: enabled, disabled: rows.length - enabled, card: card, network: network, emu: emu };\n" \
	"    Object.keys(counts).forEach(function (name) {\n" \
	"      var el = $('rcnt_' + name);\n" \
	"      if (el) el.textContent = counts[name];\n" \
	"    });\n" \
	"    var footer = $('rCount');\n" \
	"    if (footer) {\n" \
	"      footer.innerHTML = '<span><b>' + rows.length + '</b> ' + (rows.length === 1 ? 'reader' : 'readers') + '</span>' +\n" \
	"        '<span><b class=\"tg\">' + enabled + '</b> Enabled</span>' +\n" \
	"        '<span><b class=\"dim\">' + (rows.length - enabled) + '</b> Disabled</span>';\n" \
	"    }\n" \
	"  }\n" \
	"\n" \
	"  function setEmpty(show, filtered) {\n" \
	"    var empty = $('rEmpty');\n" \
	"    if (!empty) return;\n" \
	"    empty.hidden = !show;\n" \
	"    if (show) {\n" \
	"      $('reT').textContent = filtered ? 'No readers match the current filter' : 'No readers configured';\n" \
	"      $('reS').textContent = filtered ? 'Try a different search or filter.' : 'Create a reader to get started.';\n" \
	"    }\n" \
	"  }\n" \
	"\n" \
	"  function button(icon, action, label, extra) {\n" \
	"    var b = document.createElement('button');\n" \
	"    b.type = 'button';\n" \
	"    b.className = 'act-b ' + extra;\n" \
	"    b.dataset.a = action;\n" \
	"    b.title = label;\n" \
	"    b.setAttribute('aria-label', label);\n" \
	"    b.innerHTML = '<svg class=\"i\" viewBox=\"0 0 24 24\"><use href=\"#' + icon + '\"/></svg>';\n" \
	"    return b;\n" \
	"  }\n" \
	"\n" \
	"  function makeRow(reader) {\n" \
	"    var tr = document.createElement('tr');\n" \
	"    tr.className = 'rrow';\n" \
	"    tr.dataset.i = reader.index;\n" \
	"    tr.dataset.enabled = reader.enabled ? '1' : '0';\n" \
	"    tr.dataset.active = reader.active ? '1' : '0';\n" \
	"    tr.dataset.cwOk = String(+reader.cw_ok || 0);\n" \
	"    tr.dataset.cwNok = String(+reader.cw_nok || 0);\n" \
	"\n" \
	"    var stateCell = document.createElement('td');\n" \
	"    stateCell.className = 'c-rstate';\n" \
	"    stateCell.innerHTML = '<span class=\"rdot ' + (reader.active ? 'on' : (reader.enabled ? 'on' : 'off')) + '\"></span><span class=\"rstate\">' + (reader.active ? 'Active' : (reader.enabled ? 'Enabled' : 'Disabled')) + '</span>';\n" \
	"    tr.appendChild(stateCell);\n" \
	"\n" \
	"    var labelCell = document.createElement('td');\n" \
	"    labelCell.className = 'c-rlabel';\n" \
	"    var label = document.createElement('button');\n" \
	"    label.type = 'button';\n" \
	"    label.className = 'rlink';\n" \
	"    label.dataset.a = 'edit';\n" \
	"    label.title = 'Edit ' + (reader.label || ('reader' + reader.index));\n" \
	"    label.textContent = reader.label || ('reader' + reader.index);\n" \
	"    labelCell.appendChild(label);\n" \
	"    tr.appendChild(labelCell);\n" \
	"\n" \
	"    var protocolCell = document.createElement('td');\n" \
	"    protocolCell.className = 'c-rproto';\n" \
	"    var badge = document.createElement('span');\n" \
	"    badge.className = 'badge ' + (reader.kind === 'card' ? 'bcy' : reader.kind === 'emu' ? 'bvi' : 'bbl');\n" \
	"    badge.textContent = String(reader.protocol || '').toUpperCase();\n" \
	"    badge.title = kindLabel(reader.kind);\n" \
	"    protocolCell.appendChild(badge);\n" \
	"    tr.appendChild(protocolCell);\n" \
	"\n" \
	"    [['c-rdev', reader.device || '—'], ['c-rgroup', reader.groups || '—'], ['c-rcaid', reader.caid || '—']].forEach(function (item) {\n" \
	"      var cell = document.createElement('td');\n" \
	"      cell.className = item[0] + ' mono';\n" \
	"      cell.textContent = item[1];\n" \
	"      tr.appendChild(cell);\n" \
	"    });\n" \
	"\n" \
	"    [['c-rstat-ok', +reader.cw_ok || 0], ['c-rstat-nok', +reader.cw_nok || 0]].forEach(function (item) {\n" \
	"      var cell = document.createElement('td');\n" \
	"      cell.className = item[0] + ' mono';\n" \
	"      cell.textContent = String(item[1]);\n" \
	"      tr.appendChild(cell);\n" \
	"    });\n" \
	"\n" \
	"\n" \
	"    var actions = document.createElement('td');\n" \
	"    actions.className = 'c-btn';\n" \
	"    var buttons = document.createElement('div');\n" \
	"    buttons.className = 'ba';\n" \
	"    buttons.appendChild(button('i-power', 'toggle', reader.enabled ? 'Disable reader' : 'Enable reader', reader.enabled ? 'on' : 'off'));\n" \
	"    buttons.appendChild(button('i-edit', 'edit', 'Edit reader', 'ed'));\n" \
	"    buttons.appendChild(button('i-trash', 'delete', 'Delete reader', 'dl'));\n" \
	"    actions.appendChild(buttons);\n" \
	"    tr.appendChild(actions);\n" \
	"    return tr;\n" \
	"  }\n" \
	"\n" \
	"  function render(animated) {\n" \
	"    updateCounts();\n" \
	"    var visible = rows.filter(matches).sort(sortReaders);\n" \
	"    body.textContent = '';\n" \
	"    var fragment = document.createDocumentFragment();\n" \
	"    visible.forEach(function (reader) { fragment.appendChild(makeRow(reader)); });\n" \
	"    body.appendChild(fragment);\n" \
	"    $('rViewStat').textContent = visible.length + ' shown';\n" \
	"    setEmpty(visible.length === 0, rows.length > 0);\n" \
	"\n" \
	"    document.querySelectorAll('#rStats .ust').forEach(function (buttonEl) {\n" \
	"      var active = buttonEl.dataset.f === state.f;\n" \
	"      buttonEl.classList.toggle('act', active);\n" \
	"      buttonEl.setAttribute('aria-pressed', active ? 'true' : 'false');\n" \
	"    });\n" \
	"\n" \
	"    document.querySelectorAll('#rTable th[data-k] .sort-arrow').forEach(function (arrow) {\n" \
	"      arrow.textContent = '';\n" \
	"      if (arrow.parentNode.parentNode.dataset.k === state.sort) arrow.textContent = state.dir === 1 ? '↑' : '↓';\n" \
	"    });\n" \
	"\n" \
	"    if (animated) {\n" \
	"      Array.prototype.forEach.call(body.querySelectorAll('.rrow'), function (row, index) {\n" \
	"        row.style.animationDelay = Math.min(index, 8) * 18 + 'ms';\n" \
	"      });\n" \
	"    }\n" \
	"  }\n" \
	"\n" \
	"  function schedule() {\n" \
	"    clearTimeout(timer);\n" \
	"    timer = setTimeout(sync, document.hidden ? 15000 : 5000);\n" \
	"  }\n" \
	"\n" \
	"  function sync() {\n" \
	"    if (busy) return;\n" \
	"    busy = true;\n" \
	"    var refresh = document.querySelector('[data-a=\"refresh\"]');\n" \
	"    if (refresh) refresh.disabled = true;\n" \
	"    tcmg_api('/api/readers').then(function (data) {\n" \
	"      rows = Array.isArray(data.readers) ? data.readers : [];\n" \
	"      render(false);\n" \
	"      $('rSync').textContent = 'Updated ' + new Date().toLocaleTimeString();\n" \
	"    }).catch(function (error) {\n" \
	"      if (!error || error.message !== 'unauthorized') toast('Reader refresh failed', 'err');\n" \
	"    }).finally(function () {\n" \
	"      busy = false;\n" \
	"      if (refresh) refresh.disabled = false;\n" \
	"      schedule();\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function toggleReader(index) {\n" \
	"    tcmg_api('/api/reader/toggle?index=' + encodeURIComponent(index), { method: 'POST' }).then(function (data) {\n" \
	"      if (!data.ok) throw new Error(data.msg || 'Toggle failed');\n" \
	"      sync();\n" \
	"      toast(data.enabled ? 'Reader enabled' : 'Reader disabled', 'ok');\n" \
	"    }).catch(function (error) {\n" \
	"      if (error.message !== 'unauthorized') toast(error.message || 'Toggle failed', 'err');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function deleteReader(index) {\n" \
	"    if (!window.confirm('Delete this reader?')) return;\n" \
	"    tcmg_api('/api/reader/delete?index=' + encodeURIComponent(index), { method: 'POST' }).then(function (data) {\n" \
	"      if (!data.ok) throw new Error(data.msg || 'Delete failed');\n" \
	"      sync();\n" \
	"      toast('Reader deleted', 'ok');\n" \
	"    }).catch(function (error) {\n" \
	"      if (error.message !== 'unauthorized') toast(error.message || 'Delete failed', 'err');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function resetReaderForm() {\n" \
	"    $('rIndex').value = '-1';\n" \
	"    $('rTitle').textContent = 'Add Reader';\n" \
	"    $('rLabel').value = '';\n" \
	"    $('rProtocol').value = 'cccam';\n" \
	"    ['rCccamDevice', 'rPcscDevice', 'rSerialDevice', 'rUser', 'rPassword', 'rKey', 'rCaid', 'rSid', 'rKeys'].forEach(function (id) { $(id).value = ''; });\n" \
	"    $('rInternalDevice').value = '/dev/sci0';\n" \
	"    $('rTimeout').value = 30;\n" \
	"    $('rGroup').value = 1;\n" \
	"    $('rWl').value = '37';\n" \
	"    ['rPoll', 'rSerialPoll'].forEach(function (id) { $(id).value = 250; });\n" \
	"    $('rFast').value = 0;\n    $('rInternalFast').value = 0;\n    $('rSerialFast').value = 0;\n" \
	"    $('rEnabled').checked = true;\n" \
	"    $('rDoEcm').checked = true;\n" \
	"    $('rInternalDoEcm').checked = true;\n" \
	"    $('rSerialDoEcm').checked = true;\n" \
	"    clearReaderError();\n" \
	"  }\n" \
	"\n" \
	"  function openReader(index) {\n" \
	"    if (index < 0) {\n" \
	"      resetReaderForm();\n" \
	"      $('rModal').hidden = false;\n" \
	"      document.body.classList.add('mo-open');\n" \
	"      syncReaderProtocol();\n" \
	"      return;\n" \
	"    }\n" \
	"    tcmg_api('/api/reader/get?index=' + encodeURIComponent(index)).then(function (data) {\n" \
	"      $('rIndex').value = data.index;\n" \
	"      $('rTitle').textContent = 'Edit Reader';\n" \
	"      $('rLabel').value = data.label || '';\n" \
	"      $('rProtocol').value = (data.protocol || 'emu').toLowerCase();\n" \
	"      ['rCccamDevice', 'rPcscDevice', 'rInternalDevice', 'rSerialDevice'].forEach(function (id) { $(id).value = ''; });\n" \
	"      if (data.protocol === 'pcsc') $('rPcscDevice').value = data.device || ''; else if (data.protocol === 'internal') $('rInternalDevice').value = data.device || ''; else if (data.protocol === 'serial') $('rSerialDevice').value = data.device || ''; else $('rCccamDevice').value = data.device || '';\n" \
	"      $('rUser').value = data.user || '';\n" \
	"      $('rPassword').value = data.password || '';\n" \
	"      $('rKey').value = data.key || '';\n" \
	"      $('rTimeout').value = data.inactivitytimeout || 30;\n" \
	"      $('rGroup').value = data.groups || data.group || '1';\n" \
	"      $('rCaid').value = data.caid || '';\n" \
	"      $('rSid').value = data.sid_whitelist || '';\n" \
	"      $('rWl').value = data.ecmwhitelist || '37';\n" \
	"      $('rPoll').value = data.protocol === 'pcsc' ? (data.POLL_MS || 250) : 250; $('rSerialPoll').value = data.protocol === 'serial' ? (data.POLL_MS || 250) : 250;\n" \
	"      $('rFast').value = data.protocol === 'pcsc' ? (data.FAST_RESET || 0) : 0; $('rInternalFast').value = data.protocol === 'internal' ? (data.FAST_RESET || 0) : 0; $('rSerialFast').value = data.protocol === 'serial' ? (data.FAST_RESET || 0) : 0;\n" \
	"      $('rInternalDoEcm').checked = !!data.DO_ECM;\n" \
	"      $('rSerialDoEcm').checked = !!data.DO_ECM;\n" \
	"      $('rEnabled').checked = !!data.enabled;\n" \
	"      $('rDoEcm').checked = !!data.DO_ECM;\n" \
	"      $('rKeys').value = data.ecmkeys || '';\n" \
	"      clearReaderError();\n" \
	"      $('rModal').hidden = false;\n" \
	"      document.body.classList.add('mo-open');\n" \
	"      syncReaderProtocol();\n" \
	"    }).catch(function (error) {\n" \
	"      if (error.message !== 'unauthorized') toast(error.message || 'Request failed', 'err');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function closeReader() {\n" \
	"    $('rModal').hidden = true;\n" \
	"    document.body.classList.remove('mo-open');\n" \
	"  }\n" \
	"\n" \
	"  function caidList(value) {\n" \
	"    var parts = String(value || '').split(/[,; ]+/).filter(Boolean), result = [];\n" \
	"    for (var i = 0; i < parts.length; i++) {\n" \
	"      if (!/^[0-9a-fA-F]{1,4}$/.test(parts[i])) return null;\n" \
	"      var caid = ('0000' + parts[i].toUpperCase()).slice(-4);\n" \
	"      if (result.indexOf(caid) < 0) result.push(caid);\n" \
	"    }\n" \
	"    return result;\n" \
	"  }\n" \
	"\n" \
	"  function keyCaids() {\n" \
	"    var result = [], match, re = /^\\s*([0-9A-Fa-f]{1,4})\\s*=/gm, text = $('rKeys').value;\n" \
	"    while ((match = re.exec(text))) {\n" \
	"      var caid = ('0000' + match[1].toUpperCase()).slice(-4);\n" \
	"      if (result.indexOf(caid) < 0) result.push(caid);\n" \
	"    }\n" \
	"    return result;\n" \
	"  }\n" \
	"\n" \
	"  function clearReaderError() { $('rErr').hidden = true; }\n" \
	"  function readerError(message) { $('rErrMsg').textContent = message; $('rErr').hidden = false; }\n" \
	"  function syncCaidPlaceholder() {\n" \
	"    if ($('rProtocol').value === 'emu') {\n" \
	"      var caids = keyCaids();\n" \
	"      $('rCaid').placeholder = caids.length ? caids.join(',') : 'Auto from ECM keys';\n" \
	"    }\n" \
	"  }\n" \
	"\n" \
	"  function loadSerialPorts() {\n" \
	"    tcmg_api('/api/serial/ports').then(function (data) {\n" \
	"      var list = $('rSerialPorts');\n" \
	"      list.textContent = '';\n" \
	"      (data.ports || []).forEach(function (port) {\n" \
	"        var option = document.createElement('option');\n" \
	"        option.value = port;\n" \
	"        list.appendChild(option);\n" \
	"      });\n" \
	"    }).catch(function () {});\n" \
	"  }\n" \
	"\n" \
	"  function syncReaderProtocol() {\n" \
	"    var protocol = $('rProtocol').value;\n" \
	"    var network = /^(cccam|mgcamd|newcamd|cs378x)$/.test(protocol);\n" \
	"    var showCc = /^(cccam|newcamd|cs378x)$/.test(protocol);\n" \
	"    var card = /^(pcsc|internal|serial)$/.test(protocol);\n" \
	"    $('rCccamFields').hidden = !showCc;\n" \
	"    $('rPcscFields').hidden = protocol !== 'pcsc';\n" \
	"    $('rInternalFields').hidden = protocol !== 'internal';\n" \
	"    $('rSerialFields').hidden = protocol !== 'serial';\n" \
	"    $('rEmuFields').hidden = protocol !== 'emu';\n" \
	"    $('rKeyWrap').hidden = !/^(mgcamd|newcamd)$/.test(protocol);\n" \
	"    $('rCaidHint').hidden = protocol !== 'emu';\n" \
	"    $('rUser').disabled = !network;\n" \
	"    $('rPassword').disabled = !network;\n" \
	"    $('rTimeout').disabled = !network;\n" \
	"    $('rKey').disabled = !/^(mgcamd|newcamd)$/.test(protocol);\n" \
	"    $('rCccamDevice').disabled = !network;\n" \
	"    $('rPcscDevice').disabled = protocol !== 'pcsc';\n" \
	"    $('rInternalDevice').disabled = protocol !== 'internal';\n" \
	"    $('rSerialDevice').disabled = protocol !== 'serial';\n" \
	"    $('rPoll').disabled = protocol !== 'pcsc';\n" \
	"    $('rFast').disabled = protocol !== 'pcsc';\n" \
	"    $('rDoEcm').disabled = protocol !== 'pcsc';\n" \
	"    $('rInternalFast').disabled = protocol !== 'internal';\n" \
	"    $('rInternalDoEcm').disabled = protocol !== 'internal';\n" \
	"    $('rSerialPoll').disabled = protocol !== 'serial';\n" \
	"    $('rSerialFast').disabled = protocol !== 'serial';\n" \
	"    $('rSerialDoEcm').disabled = protocol !== 'serial';\n" \
	"    $('rKeys').disabled = protocol !== 'emu';\n" \
	"    if (card && protocol === 'serial') loadSerialPorts();\n" \
	"    if (protocol !== 'pcsc') $('rPoll').value = 250; if (protocol !== 'internal') $('rInternalFast').value = 0; if (protocol !== 'serial') $('rSerialPoll').value = 250; if (protocol !== 'serial') $('rSerialFast').value = 0;\n" \
	"    syncCaidPlaceholder();\n" \
	"  }\n" \
	"\n" \
	"  function saveReader() {\n" \
	"    clearReaderError();\n" \
	"    var protocol = $('rProtocol').value;\n" \
	"    var request = new URLSearchParams();\n" \
	"    var network = /^(cccam|mgcamd|newcamd|cs378x)$/.test(protocol);\n" \
	"    request.set('index', $('rIndex').value);\n" \
	"    request.set('label', $('rLabel').value.trim());\n" \
	"    request.set('protocol', protocol);\n" \
	"    request.set('device', protocol === 'pcsc' ? $('rPcscDevice').value.trim() : protocol === 'internal' ? $('rInternalDevice').value.trim() : protocol === 'serial' ? $('rSerialDevice').value.trim() : network ? $('rCccamDevice').value.trim() : '');\n" \
	"    request.set('user', network ? $('rUser').value : '');\n" \
	"    request.set('password', network ? $('rPassword').value : '');\n" \
	"    request.set('key', /^(mgcamd|newcamd)$/.test(protocol) ? $('rKey').value.trim() : '');\n" \
	"    request.set('inactivitytimeout', network ? $('rTimeout').value : '30');\n" \
	"    var caids = caidList($('rCaid').value);\n" \
	"    if (!caids) { readerError('CAID must be 1-4 hex digits separated by commas'); $('rCaid').focus(); return; }\n" \
	"    if (caids.length > 8) { readerError('At most 8 CAIDs per reader'); $('rCaid').focus(); return; }\n" \
	"    request.set('group', $('rGroup').value.trim());\n" \
	"    request.set('caid', caids.join(','));\n" \
	"    request.set('sid_whitelist', $('rSid').value.trim());\n" \
	"    request.set('ecmwhitelist', $('rWl').value.trim());\n" \
	"    request.set('ecmkeys', protocol === 'emu' ? $('rKeys').value : '');\n" \
	"    request.set('DO_ECM', protocol === 'pcsc' ? ($('rDoEcm').checked ? '1' : '0') : protocol === 'internal' ? ($('rInternalDoEcm').checked ? '1' : '0') : protocol === 'serial' ? ($('rSerialDoEcm').checked ? '1' : '0') : '1');\n" \
	"    request.set('FAST_RESET', protocol === 'pcsc' ? $('rFast').value : protocol === 'internal' ? $('rInternalFast').value : protocol === 'serial' ? $('rSerialFast').value : '0');\n" \
	"    if (protocol === 'pcsc') request.set('POLL_MS', $('rPoll').value); else if (protocol === 'serial') request.set('POLL_MS', $('rSerialPoll').value);\n" \
	"    request.set('enabled', $('rEnabled').checked ? '1' : '0');\n" \
	"\n" \
	"    var save = $('rSave');\n" \
	"    save.disabled = true;\n" \
	"    tcmg_api('/api/reader/save', { method: 'POST', body: request.toString(), headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })\n" \
	"      .then(function (data) { if (data.ok) { closeReader(); sync(); } else readerError(data.msg || 'Save failed'); })\n" \
	"      .catch(function (error) { if (error.message !== 'unauthorized') readerError(error.message || 'Request failed'); })\n" \
	"      .finally(function () { save.disabled = false; });\n" \
	"  }\n" \
	"\n" \
	"  $('rSearch').addEventListener('focus', function () { this.removeAttribute('readonly'); });\n" \
	"  $('rSearch').addEventListener('input', function () { state.q = this.value.trim(); render(false); });\n" \
	"  $('rStats').addEventListener('click', function (event) {\n" \
	"    var filter = event.target.closest('[data-f]');\n" \
	"    if (!filter) return;\n" \
	"    state.f = filter.dataset.f;\n" \
	"    render(false);\n" \
	"  });\n" \
	"  document.querySelector('#rTable thead').addEventListener('click', function (event) {\n" \
	"    var header = event.target.closest('th[data-k]');\n" \
	"    if (!header) return;\n" \
	"    if (state.sort === header.dataset.k) state.dir *= -1;\n" \
	"    else { state.sort = header.dataset.k; state.dir = 1; }\n" \
	"    render(false);\n" \
	"  });\n" \
	"  body.addEventListener('click', function (event) {\n" \
	"    var action = event.target.closest('[data-a]');\n" \
	"    if (!action) return;\n" \
	"    var row = action.closest('.rrow');\n" \
	"    if (!row) return;\n" \
	"    var index = parseInt(row.dataset.i, 10);\n" \
	"    if (action.dataset.a === 'edit') openReader(index);\n" \
	"    else if (action.dataset.a === 'toggle') toggleReader(index);\n" \
	"    else if (action.dataset.a === 'delete') deleteReader(index);\n" \
	"  });\n" \
	"  document.querySelector('[data-a=\"refresh\"]').addEventListener('click', sync);\n" \
	"  document.querySelectorAll('[data-a=\"add\"]').forEach(function (buttonEl) { buttonEl.addEventListener('click', function () { openReader(-1); }); });\n" \
	"  $('rProtocol').addEventListener('change', syncReaderProtocol);\n" \
	"  $('rKeys').addEventListener('input', syncCaidPlaceholder);\n" \
	"  document.addEventListener('keydown', function (event) { if (event.key === 'Escape' && !$('rModal').hidden) closeReader(); });\n" \
	"  document.addEventListener('visibilitychange', schedule);\n" \
	"\n" \
	"  window.openReader = openReader;\n" \
	"  window.closeReader = closeReader;\n" \
	"  window.saveReader = saveReader;\n" \
	"  window.syncReaderProtocol = syncReaderProtocol;\n" \
	"\n" \
	"  sync();\n" \
	"})();\n"
#endif

-- ==========================================
-- Rainbow Moving KBRON PRO v12.1 - + Direcciones desde/hacia el centro
-- ==========================================
script_name = "ArcoirisFX"
script_description = "Presets Reales + Memoria + Frame/Single + Alpha Check + Centro."
script_author = "GH2382"
script_version = "12.1"

local json = require("json")
local cfg_file = aegisub.decode_path("?user/kbron_v12.json")

-- Nombres de las direcciones (se usan en el dropdown y en la lógica)
local DIR_LTR        = "LTR"
local DIR_RTL        = "RTL"
local DIR_TO_CENTER  = "Lados → Centro"
local DIR_FROM_CENTER = "Centro → Lados"

-- Función de clonación profunda
local function deep_copy(obj)
    if type(obj) ~= 'table' then return obj end
    local res = {}
    for k, v in pairs(obj) do res[deep_copy(k)] = deep_copy(v) end
    return res
end

-- Cargar Datos (Presets + Última Configuración)
local function load_data()
    local f = io.open(cfg_file, "r")
    if f then
        local content = f:read("*all")
        f:close()
        local ok, data = pcall(json.decode, content)
        if ok then return data end
    end
    -- Valores por defecto si no hay archivo
    return { 
        last = "Default",
        presets = { 
            ["Default"] = {dur=60, steps=3, x_chars=1, dir="LTR", mode="Varias Líneas (Frame)", bord=2.5, shad=2.5, colors={"#FF0000","#FFFF00","#00FF00","#00FFFF","#0000FF","#FF00FF"}} 
        }
    }
end

local function save_data(data)
    local f = io.open(cfg_file, "w")
    if f then f:write(json.encode(data)) f:close() end
end

local function hex_to_rgb(hex)
    hex = hex:gsub("#","")
    return tonumber(hex:sub(1,2),16) or 255, tonumber(hex:sub(3,4),16) or 255, tonumber(hex:sub(5,6),16) or 255
end

local function rgb_to_ass(r, g, b)
    return string.format("&H%02X%02X%02X&", math.floor(b), math.floor(g), math.floor(r))
end

-- Cuenta los caracteres visibles (mismas reglas que el procesamiento:
-- ignora tags {}, espacios y texto invisible con alpha&HFF&)
local function count_visible(text)
    local n, p, inv = 0, 1, false
    while p <= #text do
        local c = text:match("^[%z\1-\127\194-\244][\128-\191]*", p) or text:sub(p,p)
        if c == "{" then
            local eb = text:find("}", p) or #text
            local tag = text:sub(p, eb)
            if tag:match("alpha&HFF&") then inv = true end
            if tag:match("alpha&H00&") then inv = false end
            p = eb + 1
        elseif c:match("%s") or inv then
            p = p + #c
        else
            n = n + 1
            p = p + #c
        end
    end
    return n
end

-- Devuelve (base, sign) para un carácter:
--   base = índice de grupo del carácter (según su posición o su distancia al centro)
--   sign = sentido en que avanza el color con cada frame/paso
local function dir_params(dir, c_idx, n_total, x_chars)
    local xc = math.max(1, x_chars)
    if dir == DIR_TO_CENTER or dir == DIR_FROM_CENTER then
        local center = (n_total + 1) / 2
        local dist = math.abs(c_idx - center)
        local base = math.floor(dist / xc)
        -- Lados → Centro: el color viaja hacia menor distancia (hacia el centro)
        -- Centro → Lados: el color viaja hacia mayor distancia (hacia los lados)
        return base, (dir == DIR_TO_CENTER) and 1 or -1
    end
    local base = math.floor((c_idx - 1) / xc)
    return base, (dir == DIR_RTL) and -1 or 1
end

function main_macro(subs, sel)
    local data = load_data()
    local preset_names = {}
    for k in pairs(data.presets) do table.insert(preset_names, k) end
    table.sort(preset_names)

    -- PASO 1: SELECCIÓN DE PRESET
    local sel_config = {
        {class="label", label="Selecciona un Preset para cargar:", x=0, y=0},
        {class="dropdown", name="target", items=preset_names, value=data.last, x=0, y=1}
    }
    local s_btn, s_res = aegisub.dialog.display(sel_config, {"Cargar", "Nuevo/Borrar", "Cancelar"})
    if s_btn ~= "Cargar" and s_btn ~= "Nuevo/Borrar" then aegisub.cancel() end

    local active_p = data.presets[s_res.target] or data.presets["Default"]
    data.last = s_res.target

    -- PASO 2: HUD PRINCIPAL CON DATOS CARGADOS
    local config = {
        {class="label", label="Configurando: " .. s_res.target, x=0, y=0, width=4},
        {class="label", label="Modo:", x=0, y=1},
        {class="dropdown", name="mode", items={"1 sola línea (\\t)","Varias Líneas (Frame)"}, value=active_p.mode or "Varias Líneas (Frame)", x=1, y=1, width=2},
        {class="label", label="Nombre Preset:", x=0, y=2},
        {class="edit", name="p_name", value=s_res.target, x=1, y=2, width=2},
    }

    -- Colores
    for i = 1, 8 do
        local c_val = (active_p.colors and active_p.colors[i]) or "#FFFFFF"
        table.insert(config, {class="checkbox", name="act"..i, label=tostring(i), value=(active_p.colors and active_p.colors[i] ~= nil or i<=6), x=0, y=2+i})
        table.insert(config, {class="color", name="c"..i, value=c_val, x=1, y=2+i})
    end

    local y = 11
    table.insert(config, {class="label", label="Borde:", x=0, y=y})
    table.insert(config, {class="floatedit", name="bord", value=active_p.bord or 2.5, x=1, y=y})
    table.insert(config, {class="label", label="Sombra:", x=2, y=y})
    table.insert(config, {class="floatedit", name="shad", value=active_p.shad or 2.5, x=3, y=y})

    table.insert(config, {class="label", label="Velocidad (ms):", x=0, y=y+1})
    table.insert(config, {class="intedit", name="dur", value=active_p.dur or 60, x=1, y=y+1})
    table.insert(config, {class="label", label="Steps:", x=2, y=y+1})
    table.insert(config, {class="intedit", name="steps", value=active_p.steps or 3, x=3, y=y+1})

    table.insert(config, {class="label", label="Cada X chars:", x=0, y=y+2})
    table.insert(config, {class="intedit", name="x_chars", value=active_p.x_chars or 1, x=1, y=y+2})
    table.insert(config, {class="label", label="Dirección:", x=2, y=y+2})
    table.insert(config, {class="dropdown", name="dir", items={DIR_LTR, DIR_RTL, DIR_TO_CENTER, DIR_FROM_CENTER}, value=active_p.dir or DIR_LTR, x=3, y=y+2})

    local btn, res = aegisub.dialog.display(config, {"🚀 Aplicar", "💾 Guardar Preset", "Cancelar"})
    
    if btn == "💾 Guardar Preset" then
        local new_p = { dur=res.dur, steps=res.steps, x_chars=res.x_chars, dir=res.dir, mode=res.mode, bord=res.bord, shad=res.shad, colors={} }
        for i=1,8 do if res["act"..i] then table.insert(new_p.colors, res["c"..i]) end end
        data.presets[res.p_name] = new_p
        data.last = res.p_name
        save_data(data)
        return main_macro(subs, sel) -- Reiniciar para ver cambios
    end
    if btn ~= "🚀 Aplicar" then aegisub.cancel() end

    -- Guardar el último estado automáticamente
    active_p.dur, active_p.steps, active_p.x_chars = res.dur, res.steps, res.x_chars
    active_p.dir, active_p.mode, active_p.bord, active_p.shad = res.dir, res.mode, res.bord, res.shad
    active_p.colors = {}
    for i=1,8 do if res["act"..i] then table.insert(active_p.colors, res["c"..i]) end end
    save_data(data)

    -- PROCESAMIENTO
    local active = {}
    for i=1,#active_p.colors do
        local r,g,b = hex_to_rgb(active_p.colors[i])
        local s_hex = string.format("#%02X%02X%02X", math.floor(r*0.3), math.floor(g*0.3), math.floor(b*0.3))
        table.insert(active, {c=active_p.colors[i], s=s_hex, b=res.bord, sh=res.shad})
    end

    local palette = {}
    for i = 1, #active do
        local next_i = (i % #active) + 1
        local r1,g1,b1 = hex_to_rgb(active[i].c)
        local r2,g2,b2 = hex_to_rgb(active[next_i].c)
        for s = 0, res.steps-1 do
            local pct = s / res.steps
            table.insert(palette, {
                c = rgb_to_ass(r1+(r2-r1)*pct, g1+(g2-g1)*pct, b1+(b2-b1)*pct),
                s = rgb_to_ass((r1+(r2-r1)*pct)*0.3, (g1+(g2-g1)*pct)*0.3, (b1+(b2-b1)*pct)*0.3),
                b = active[i].b, sh = active[i].sh
            })
        end
    end

    -- Aplicación a las líneas
    for i = #sel, 1, -1 do
        local l_idx = sel[i]
        local line = subs[l_idx]
        local dur_total = line.end_time - line.start_time
        local n_total = count_visible(line.text)  -- necesario para las direcciones de centro
        
        if res.mode == "1 sola línea (\\t)" then
            local ft, c_idx, p = "", 0, 1
            local is_inv = false
            while p <= #line.text do
                local c = line.text:match("^[%z\1-\127\194-\244][\128-\191]*", p) or line.text:sub(p,p)
                if c == "{" then
                    local eb = line.text:find("}", p) or #line.text
                    local tag = line.text:sub(p, eb)
                    if tag:match("alpha&HFF&") then is_inv = true end
                    if tag:match("alpha&H00&") then is_inv = false end
                    ft = ft .. tag p = eb + 1
                elseif c:match("%s") or is_inv then ft = ft .. c p = p + #c
                else
                    c_idx = c_idx + 1
                    local base, sign = dir_params(res.dir, c_idx, n_total, res.x_chars)
                    local ptr = (base % #palette) + 1
                    local d = palette[ptr]
                    local tags = string.format("{\\1c%s\\3c%s\\4c%s\\bord%g\\shad%g", d.c, d.s, d.s, d.b, d.sh)
                    local t, stp = 0, 1
                    while t < dur_total do
                        local n_ptr = ((ptr + (stp * sign) - 1) % #palette) + 1
                        local nd = palette[n_ptr]
                        tags = tags .. string.format("\\t(%d,%d,\\1c%s\\3c%s\\4c%s)", t, t+res.dur, nd.c, nd.s, nd.s)
                        t, stp = t + res.dur, stp + 1
                    end
                    ft = ft .. tags .. "}" .. c p = p + #c
                end
            end
            line.text = ft:gsub("}{", "") subs[l_idx] = line
        else
            local frames = math.ceil(dur_total / res.dur)
            for f = 0, frames - 1 do
                local clone = deep_copy(line)
                clone.start_time, clone.end_time = line.start_time + (f*res.dur), math.min(line.end_time, line.start_time+(f+1)*res.dur)
                local ft, c_idx, p, is_inv = "", 0, 1, false
                while p <= #line.text do
                    local c = line.text:match("^[%z\1-\127\194-\244][\128-\191]*", p) or line.text:sub(p,p)
                    if c == "{" then
                        local eb = line.text:find("}", p) or #line.text
                        local tag = line.text:sub(p, eb)
                        if tag:match("alpha&HFF&") then is_inv = true end
                        if tag:match("alpha&H00&") then is_inv = false end
                        ft = ft .. tag p = eb + 1
                    elseif c:match("%s") or is_inv then ft = ft .. c p = p + #c
                    else
                        c_idx = c_idx + 1
                        local base, sign = dir_params(res.dir, c_idx, n_total, res.x_chars)
                        local ptr = ((base + (f * sign)) % #palette) + 1
                        if ptr <= 0 then ptr = ptr + #palette end
                        local d = palette[ptr]
                        ft = ft .. string.format("{\\1c%s\\3c%s\\4c%s\\bord%g\\shad%g}", d.c, d.s, d.s, d.b, d.sh) .. c p = p + #c
                    end
                end
                clone.text = ft:gsub("}{", "") subs.insert(l_idx + f + 1, clone)
            end
            subs.delete(l_idx)
        end
    end
    aegisub.set_undo_point(script_name)
end

aegisub.register_macro(script_name, script_description, main_macro)
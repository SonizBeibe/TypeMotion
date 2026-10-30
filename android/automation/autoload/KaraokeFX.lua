-- ==========================================
-- Karaoke FX Suite (Unificado y Optimizado)
-- ==========================================
script_name = "Karaoke FX"
script_description = "Efectos de Karaoke Unificados, empaquetados en carpeta nativa de arch1t3ct"
script_author = "Soniz / Custom"
script_version = "1.8"

-- ==========================================
-- Configuración
-- ==========================================
local config_file = aegisub.decode_path("?user/") .. "karaokefx_suite_cfg.txt"

local function cargar_config()
    local cfg = {
        guardar       = true,
        preserve_grad = false,

        -- Efecto (página) seleccionado: 1 Color Fade, 2 Movement, 3 Fluido, 4 Reverso
        efecto_idx    = 1,
        comb_mov      = false, -- combinar el efecto elegido con Movement (cascada)

        -- 1. Color Fade
        col_glow      = "#FFFFFF",
        fade_dur      = 300,

        -- 2. Movement
        dir_idx       = 1,
        duracion      = 300,
        distancia     = 20,

        -- 3. Fluido (Aparecer)
        fluido_dur    = 300,
        fluido_prim   = false,

        -- 4. Reverso (Desaparecer)
        rev_dur       = 300,

        -- 5. Destacado
        dest_tam      = true,  -- usar el tamaño de la sílaba resaltada (false: se queda en tamaño base)
        dest_fs       = 80,    -- tamaño (fs) de la sílaba resaltada
        dest_completa = true,  -- true: línea completa con la sílaba activa resaltada; false: aparece como karaoke
        comb_fade     = true,  -- combinar con Color Fade (usa los ajustes de la página 1)

        -- 6. Central
        central_var       = 1,     -- variación (1 a 5)
        central_big       = 130,   -- escala mayor a la del estilo (%), variaciones 3 y 4
        central_small     = 60,    -- escala menor a la del estilo (%), variación 5
        central_zoom_dur  = 250,   -- duración del zoom (ms)
        central_completa  = false, -- true: línea completa visible; false: aparece sílaba por sílaba

        pos_x = 640, pos_y = 700,
    }
    local file = io.open(config_file, "r")
    if file then
        for line in file:lines() do
            local k, v = string.match(line, "([^=]+)=(.+)")
            if k and v then
                if k == "col_glow" then cfg[k] = v
                elseif v == "true" then cfg[k] = true
                elseif v == "false" then cfg[k] = false
                elseif tonumber(v) then cfg[k] = tonumber(v)
                end
            end
        end
        file:close()
    end
    -- Claves de versiones anteriores (ahora se calculan según la página elegida)
    cfg.color_fade, cfg.movement, cfg.fluido, cfg.reverse = nil, nil, nil, nil
    return cfg
end

local function guardar_config(cfg)
    if not cfg.guardar then return end
    local file = io.open(config_file, "w")
    if file then
        for k, v in pairs(cfg) do
            file:write(k .. "=" .. tostring(v) .. "\n")
        end
        file:close()
    end
end

-- ==========================================
-- Utilidades
-- ==========================================
local function clonar_linea(linea)
    local nueva = {}
    for k, v in pairs(linea) do nueva[k] = v end
    return nueva
end

-- clonar_linea() es una copia superficial: si no le damos a "extra" una tabla
-- nueva, todas las líneas clonadas de la misma línea original terminan
-- COMPARTIENDO la misma tabla de metadatos por referencia. Esto rompe el
-- folder si alguna vez se le agregan datos de carpeta a una copia. Esta
-- función siempre devuelve una tabla nueva e independiente.
local function extra_independiente(origen, excluir_clave)
    local nueva = {}
    if type(origen) == "table" then
        for k, v in pairs(origen) do
            if k ~= excluir_clave then nueva[k] = v end
        end
    end
    return nueva
end

-- Variaciones de la página "Karaoke Central" (se usan en la UI y en el nombre de la carpeta)
local central_vars = {
    "1. Brillo por sílaba",
    "2. Brillo a la línea completa",
    "3. Brillo y zoom a la sílaba",
    "4. Brillo y zoom a la línea completa",
    "5. Aparición en escala (sin brillo)",
}

-- Nombre legible del/los efecto(s) activo(s), usado como título de la carpeta
local function nombre_efecto_activo(cfg)
    local partes = {}
    if cfg.color_fade then table.insert(partes, "Color Fade") end
    if cfg.movement then table.insert(partes, "Movement") end
    if cfg.fluido then table.insert(partes, "Fluido") end
    if cfg.reverse then table.insert(partes, "Reverso") end
    if cfg.destacado then table.insert(partes, "Destacado") end
    if cfg.central then table.insert(partes, "Central (" .. (central_vars[cfg.central_var] or "?") .. ")") end
    if #partes == 0 then return "Normal" end
    return table.concat(partes, " + ")
end

-- \pos(x,y) con espacios opcionales; captura x e y
local PAT_POS = "\\pos%(%s*([%d%.%-]+)%s*,%s*([%d%.%-]+)%s*%)"

local function get_line_pos(line, styles, playres_x, playres_y)
    local px, py = line.text:match(PAT_POS)
    if px and py then return tonumber(px), tonumber(py) end

    local style = styles[line.style] or {}
    local align = tonumber(line.text:match("^{\\[^}]*\\an(%d)")) or tonumber(line.text:match("\\an(%d)")) or style.align or 2

    local ml = (line.margin_l and line.margin_l > 0) and line.margin_l or (style.margin_l or 0)
    local mr = (line.margin_r and line.margin_r > 0) and line.margin_r or (style.margin_r or 0)
    local mt = (line.margin_t and line.margin_t > 0) and line.margin_t or (style.margin_t or style.margin_v or 0)
    local mb = (line.margin_b and line.margin_b > 0) and line.margin_b or (style.margin_b or style.margin_v or 0)

    local x = playres_x / 2
    if align == 1 or align == 4 or align == 7 then x = ml
    elseif align == 3 or align == 6 or align == 9 then x = playres_x - mr end

    local y = playres_y - mb
    if align == 4 or align == 5 or align == 6 then y = playres_y / 2
    elseif align == 7 or align == 8 or align == 9 then y = mt end

    return x, y
end

local function hex_to_ass(hex)
    if not hex then return "&HFFFFFF&" end
    local r, g, b = string.match(hex, "#(%x%x)(%x%x)(%x%x)")
    if r and g and b then return string.format("&H%s%s%s&", b, g, r) end
    return hex
end

local function get_style_color(style_name, color_key, fallback_hex, styles)
    local s = styles[style_name]
    if s and s[color_key] then
        local hex = s[color_key]:match("&H%x%x(%x%x%x%x%x%x)")
        if hex then return "&H" .. hex .. "&" end
    end
    return hex_to_ass(fallback_hex)
end

local direcciones = {
    { label = "Abajo (cae desde arriba)",            fx = function(bx, by, d) return bx,     by - d end },
    { label = "Arriba (sube desde abajo)",           fx = function(bx, by, d) return bx,     by + d end },
    { label = "Derecha (entra desde la izquierda)",  fx = function(bx, by, d) return bx - d, by     end },
    { label = "Izquierda (entra desde la derecha)",  fx = function(bx, by, d) return bx + d, by     end },
    { label = "Diagonal ↘ (desde arriba-izquierda)", fx = function(bx, by, d) return bx - d, by - d end },
    { label = "Diagonal ↙ (desde arriba-derecha)",   fx = function(bx, by, d) return bx + d, by - d end },
    { label = "Diagonal ↗ (desde abajo-izquierda)",  fx = function(bx, by, d) return bx - d, by + d end },
    { label = "Diagonal ↖ (desde abajo-derecha)",    fx = function(bx, by, d) return bx + d, by + d end },
}
local dir_labels = {}
for i, d in ipairs(direcciones) do dir_labels[i] = d.label end

-- ==========================================
-- Motor Principal de Procesamiento
-- ==========================================
local function procesar_karaokefx(subs, sel, cfg, styles, playres_x, playres_y, is_manual)
    local lineas_insertadas = 0
    local glow = hex_to_ass(cfg.col_glow)
    local nombre_efecto = nombre_efecto_activo(cfg)

    for i = 1, #sel do
        local indice_real = sel[i] + lineas_insertadas
        local line = subs[indice_real]

        local bx, by = cfg.pos_x, cfg.pos_y
        if not is_manual then bx, by = get_line_pos(line, styles, playres_x, playres_y) end

        -- Colores forzados del estilo nativo
        local c1 = get_style_color(line.style, "color1", "#FFFFFF", styles)
        local c2 = get_style_color(line.style, "color2", "#0000FF", styles)
        local c3 = get_style_color(line.style, "color3", "#000000", styles)
        local c4 = get_style_color(line.style, "color4", "#000000", styles)

        -- Tamaños para Karaoke Destacado. Solo se escriben si la casilla de tamaño está activa;
        -- si no, no se usa ningún \fs y todo el texto queda en el tamaño base.
        -- El tamaño base es el del estilo de la línea (si no se conoce, "\fs" sin valor = tamaño del estilo).
        local tag_fs_dest, tag_fs_base = "", ""
        if cfg.destacado and cfg.dest_tam then
            local st = styles[line.style]
            local fs_base = st and tonumber(st.fontsize)
            tag_fs_dest = string.format("\\fs%g", cfg.dest_fs)
            tag_fs_base = fs_base and string.format("\\fs%g", fs_base) or "\\fs"
        end

        -- Tags para Karaoke Central (se calculan una vez por línea). El brillo usa el color Glow y la duración
        -- de fade de la página 1; el zoom es \fs(estilo x escala)\t(0,dur,\fs(estilo)), es decir, parte de otro
        -- tamaño y se asienta en el del estilo. Si no se conoce el tamaño del estilo, no se aplica zoom.
        local cen = nil
        if cfg.central then
            local st = styles[line.style]
            local fsb = st and tonumber(st.fontsize)
            local function zoom(pct)
                if not fsb then return "" end
                return string.format("\\fs%g\\t(0,%d,\\fs%g)", fsb * pct / 100, cfg.central_zoom_dur, fsb)
            end
            cen = {
                glow  = string.format("\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)", glow, glow, glow, cfg.fade_dur, c1, c3, c4),
                reset = string.format("\\1c%s\\3c%s\\4c%s", c1, c3, c4),
                big   = zoom(cfg.central_big),
                small = zoom(cfg.central_small),
                fs    = fsb and string.format("\\fs%g", fsb) or "",
            }
        end

        -- Parseo inteligente de sílabas (respeta tags internos)
        local silabas = {}
        local clean_text = line.text:gsub("\\pos%([^%)]+%)", "")
        local p = 1
        while p <= #clean_text do
            local s, e, dur = string.find(clean_text, "{\\[kK](%d+)}", p)
            if not s then break end
            
            local next_s = string.find(clean_text, "{\\[kK]%d+}", e + 1)
            local content = ""
            if next_s then
                content = string.sub(clean_text, e + 1, next_s - 1)
                p = next_s
            else
                content = string.sub(clean_text, e + 1)
                p = #clean_text + 1
            end
            table.insert(silabas, { dur = tonumber(dur) * 10, text = content })
        end
        
        -- Encabezado de la línea: los bloques de tags que van ANTES del primer {\k} (\an, \pos, \blur...).
        -- Se copia a cada línea generada para conservar la alineación, la posición y demás.
        -- Se omiten los tags que dependen del tiempo (\fad, \fade, \move, \t), porque se reiniciarían en
        -- cada sílaba, y \pos solo se omite con Movement, que ya coloca el texto con su propio \move.
        -- El texto suelto que haya antes del primer {\k} no se copia.
        local header = ""
        local k1 = string.find(line.text, "{\\[kK]%d+}")
        if k1 and k1 > 1 then
            for blk in string.sub(line.text, 1, k1 - 1):gmatch("%b{}") do
                if blk:find("\\", 1, true) then
                    blk = blk:gsub("\\fade?%b()", ""):gsub("\\move%b()", ""):gsub("\\t%b()", "")
                    if cfg.movement then blk = blk:gsub("\\pos%b()", "") end
                    header = header .. blk
                end
            end
            header = header:gsub("{%s*}", "")
        end
        -- \pos puede estar en cualquier parte de la línea (p. ej. dentro del bloque de la primera sílaba:
        -- {\k20}{\an5\pos(640,680)}Ka). Los \pos que van dentro del texto de las sílabas se descartan, así que
        -- se toma el primero de toda la línea y se coloca en el encabezado. Con Movement no hace falta:
        -- get_line_pos ya lo usa como destino del \move.
        if not cfg.movement and not header:find("\\pos(", 1, true) then
            local px, py = line.text:match(PAT_POS)
            if px and py then header = header .. string.format("{\\pos(%s,%s)}", px, py) end
        end

        if #silabas > 0 then
            local acumulado = 0
            local lineas_nuevas = {}
            local texto_limpio = line.text:gsub("{[^}]+}", "")

            -- CREAR CARPETA (Apertura nativa con Extradata)
            local f_start = clonar_linea(line)
            f_start.layer = 0
            f_start.comment = true
            f_start.effect = "Folder: " .. nombre_efecto
            f_start.text = "Karaoke FX (" .. nombre_efecto .. "): " .. texto_limpio
            f_start.extra = { _aegi_folddata = "0;1;1" } -- FolderStart, Plegada/Colapsada; Aegisub antepone su propia 'e' al guardar
            table.insert(lineas_nuevas, f_start)

            -- RESPALDO DE LÍNEA ORIGINAL
            local orig = clonar_linea(line)
            orig.comment = true
            orig.effect = "Línea Original"
            orig.extra = extra_independiente(line.extra, "_aegi_folddata")
            table.insert(lineas_nuevas, orig)

            for s = 1, #silabas do
                local nueva_linea = clonar_linea(line)
                local s_dur = silabas[s].dur
                
                if cfg.movement then
                    nueva_linea.start_time = line.start_time + acumulado
                    -- end_time hereda el original para quedarse en pantalla (Cascada)
                else
                    nueva_linea.start_time = line.start_time + acumulado
                    nueva_linea.end_time = nueva_linea.start_time + s_dur
                end
                
                nueva_linea.effect = "Karaoke FX"
                nueva_linea.extra = extra_independiente(line.extra, "_aegi_folddata")

                local tag_move = ""
                if cfg.movement then
                    local dir = direcciones[cfg.dir_idx] or direcciones[1]
                    local start_x, start_y = dir.fx(bx, by, cfg.distancia)
                    tag_move = string.format("\\move(%.1f,%.1f,%.1f,%.1f,0,%d)", start_x, start_y, bx, by, cfg.duracion)
                end

                local texto_final = ""
                for j = 1, #silabas do
                    local t_silaba = silabas[j].text
                    
                    local pref = ""
                    if not cfg.preserve_grad and j == 1 then
                        pref = string.format("{\\1c%s\\2c%s\\3c%s\\4c%s}", c1, c2, c3, c4)
                    end

                    local mov = (cfg.movement and j == s and tag_move ~= "") and ("{"..tag_move.."}") or ""
                    
                    -- Aislamiento de Cascada para evitar superposiciones de movimiento
                    local is_visible_in_this_pass = true
                    if cfg.movement and j ~= s then
                        is_visible_in_this_pass = false
                    end

                    if not is_visible_in_this_pass then
                        texto_final = texto_final .. pref .. "{\\alpha&HFF&}" .. t_silaba
                    else
                        if cfg.central then
                            -- =====================================
                            -- 6. CENTRAL (brillo y/o zoom, por sílaba o por línea completa)
                            -- =====================================
                            -- Cada sílaba genera su propia línea, así que lo que va "a nivel de línea" (tags en la
                            -- primera sílaba, que valen para todo el texto) se dispara de nuevo con cada sílaba.
                            -- Variaciones: 1 brillo en la sílaba activa; 2 brillo en toda la línea;
                            -- 3 brillo + zoom en la sílaba activa; 4 brillo + zoom en toda la línea;
                            -- 5 aparición en escala en la sílaba activa, sin brillo.
                            -- Lo que va DESPUÉS de la activa se restablece (tamaño y color animado).
                            -- Las sílabas que faltan por cantar se ELIMINAN de la línea (no se ocultan con alpha), así
                            -- que con alineación centrada el texto va apareciendo desde el centro. En ese modo se recorta
                            -- el espacio final de la última sílaba visible para que el centrado sea exacto.
                            local v = cfg.central_var
                            local visible = cfg.central_completa or (j <= s)
                            local tags = ""
                            if j == 1 then
                                if v == 2 then
                                    tags = cen.glow
                                elseif v == 4 then
                                    tags = cen.big .. cen.glow
                                end
                            end
                            if j == s then
                                if v == 1 then
                                    tags = tags .. cen.glow
                                elseif v == 3 then
                                    tags = tags .. cen.big .. cen.glow
                                elseif v == 5 then
                                    tags = tags .. cen.small
                                end
                            elseif j > s then
                                if v == 3 or v == 5 then tags = tags .. cen.fs end
                                if v == 1 or v == 3 then tags = tags .. cen.reset end
                            end
                            if visible then
                                local txt = t_silaba
                                if j == s and not cfg.central_completa then txt = txt:gsub("%s+$", "") end
                                texto_final = texto_final .. pref .. (tags ~= "" and ("{" .. tags .. "}") or "") .. txt
                            end

                        elseif cfg.destacado then
                            -- =====================================
                            -- 5. DESTACADO (subrayado + tamaño opcional en la sílaba activa)
                            -- =====================================
                            -- Línea completa: todas las sílabas visibles y se resalta la activa.
                            -- Progresivo: solo se ven las ya cantadas (la última queda resaltada);
                            -- las que faltan se ocultan con alpha pero conservan su espacio.
                            -- El estado de los tags persiste, así que lo que viene DESPUÉS de la
                            -- activa se restablece (subrayado, tamaño y color animado).
                            local visible = cfg.dest_completa or (j <= s)
                            if j == s then
                                local tags = "\\u1" .. tag_fs_dest
                                if cfg.color_fade then
                                    tags = tags .. string.format("\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)", glow, glow, glow, cfg.fade_dur, c1, c3, c4)
                                end
                                texto_final = texto_final .. pref .. "{" .. tags .. "}" .. t_silaba
                            elseif j > s then
                                local tags = "\\u0" .. tag_fs_base
                                if not visible then
                                    tags = "\\alpha&HFF&" .. tags
                                elseif cfg.color_fade then
                                    tags = tags .. string.format("\\1c%s\\3c%s\\4c%s", c1, c3, c4)
                                end
                                texto_final = texto_final .. pref .. "{" .. tags .. "}" .. t_silaba
                            else
                                texto_final = texto_final .. pref .. t_silaba
                            end

                        elseif cfg.fluido then
                            -- =====================================
                            -- 3. FLUIDO (Aparecer progresivo)
                            -- =====================================
                            if cfg.fluido_prim then
                                -- Mantiene borde y sombra. El color base va de Secundario a Primario.
                                local target_c = c1
                                if cfg.preserve_grad then
                                    target_c = t_silaba:match("\\[1]?c(&H%x+&)") or c1
                                end
                                local clean_silaba = t_silaba
                                if cfg.preserve_grad then
                                    clean_silaba = clean_silaba:gsub("\\[1]?c&H%x+&", "")
                                end

                                if j < s then
                                    texto_final = texto_final .. pref .. "{\\1a&H00&}" .. t_silaba
                                elseif j == s then
                                    local fade = string.format("\\t(0,%d,\\1c%s)", cfg.fluido_dur, target_c)
                                    texto_final = texto_final .. pref .. mov .. "{\\1c" .. c2 .. "\\1a&H00&" .. fade .. "}" .. clean_silaba
                                else
                                    texto_final = texto_final .. pref .. "{\\1c" .. c2 .. "\\1a&H00&}" .. clean_silaba
                                end
                            else
                                -- Anima todos los canales (Alpha total)
                                if j < s then
                                    texto_final = texto_final .. pref .. "{\\alpha&H00&}" .. t_silaba
                                elseif j == s then
                                    local fade = string.format("\\t(0,%d,\\alpha&H00&)", cfg.fluido_dur)
                                    texto_final = texto_final .. pref .. mov .. "{\\alpha&HFF&" .. fade .. "}" .. t_silaba
                                else
                                    texto_final = texto_final .. pref .. "{\\alpha&HFF&}" .. t_silaba
                                end
                            end
                            
                        elseif cfg.reverse then
                            -- =====================================
                            -- 4. REVERSO (Desaparecer total)
                            -- =====================================
                            if j < s then
                                texto_final = texto_final .. pref .. "{\\alpha&HFF&}" .. t_silaba
                            elseif j == s then
                                local fade = string.format("\\t(0,%d,\\alpha&HFF&)", cfg.rev_dur)
                                texto_final = texto_final .. pref .. mov .. "{\\alpha&H00&" .. fade .. "}" .. t_silaba
                            else
                                texto_final = texto_final .. pref .. "{\\alpha&H00&}" .. t_silaba
                            end
                            
                        else
                            -- =====================================
                            -- NORMAL / COLOR FADE / MOVEMENT
                            -- =====================================
                            if j < s then
                                texto_final = texto_final .. pref .. "{\\alpha&H00&}" .. t_silaba
                            elseif j == s then
                                local c_tag = ""
                                if cfg.color_fade then
                                    c_tag = string.format("\\1c%s\\3c%s\\4c%s\\t(0,%d,\\1c%s\\3c%s\\4c%s)", glow, glow, glow, cfg.fade_dur, c1, c3, c4)
                                end
                                texto_final = texto_final .. pref .. mov .. string.format("{\\alpha&H00&%s}", c_tag) .. t_silaba
                            else
                                texto_final = texto_final .. pref .. "{\\alpha&HFF&}" .. t_silaba
                            end
                        end
                    end
                end

                texto_final = texto_final:gsub(PAT_POS, "")
                texto_final = (header .. texto_final):gsub("}{", ""):gsub("{%s*}", "")

                nueva_linea.text = texto_final
                table.insert(lineas_nuevas, nueva_linea)
                acumulado = acumulado + s_dur
            end

            -- CREAR CARPETA (Cierre nativo con Extradata)
            local f_end = clonar_linea(line)
            f_end.layer = 0
            f_end.comment = true
            f_end.effect = "EndFolder: " .. nombre_efecto
            f_end.text = ""
            f_end.extra = { _aegi_folddata = "1;1;1" } -- FolderEnd; Aegisub antepone su propia 'e' al guardar
            table.insert(lineas_nuevas, f_end)

            -- Insertar todo el paquete en Aegisub, dentro de la carpeta
            subs[indice_real] = lineas_nuevas[1]
            for j = 2, #lineas_nuevas do
                subs.insert(indice_real + j - 1, lineas_nuevas[j])
            end
            lineas_insertadas = lineas_insertadas + (#lineas_nuevas - 1)
        end
    end
    aegisub.set_undo_point("Karaoke FX Suite")
end

-- ==========================================
-- Interfaz Gráfica (UI): ventana única con una página por efecto
-- ==========================================
-- Aegisub no permite mostrar/ocultar controles dentro de un mismo diálogo, así que
-- cada "página" es el mismo diálogo vuelto a abrir con los controles del efecto
-- elegido en el selector inferior. Los valores de todas las páginas se conservan
-- entre cambios (viven en cfg).
local BTN_APLICAR  = "✔ Aplicar"
local BTN_CAMBIAR  = "↻ Cambiar efecto"
local BTN_CANCELAR = "✕ Cancelar"

local SEP = "──────────────────────────────────────────────────────────"
local FILAS_PAGINA = 4 -- alto fijo del área de contenido: la ventana no "salta" al cambiar de página

local function lbl(x, y, w, texto)
    return {class = "label", x = x, y = y, width = w, height = 1, label = texto}
end

local function check_combinar(cfg, y)
    return {class = "checkbox", name = "comb_mov", x = 0, y = y, width = 6, height = 1,
            label = "+ Combinar con Movement (usa los ajustes de la página 2)", value = cfg.comb_mov}
end

local function check_combinar_fade(cfg, y)
    return {class = "checkbox", name = "comb_fade", x = 0, y = y, width = 6, height = 1,
            label = "+ Combinar con Color Fade (usa los ajustes de la página 1)", value = cfg.comb_fade}
end

-- Cada página devuelve su lista de controles a partir de la fila y
local paginas = {
    {
        nombre = "1. Color Fade (Brillante)",
        construir = function(cfg, y)
            return {
                lbl(0, y, 6, "Cada sílaba parte del color de brillo (Glow) y se desvanece al color del estilo."),
                lbl(0, y + 1, 2, "  Color (Glow):"),
                {class = "color",   name = "col_glow", x = 2, y = y + 1, width = 1, height = 1, value = cfg.col_glow},
                lbl(3, y + 1, 2, "  Duración de fade (ms):"),
                {class = "intedit", name = "fade_dur", x = 5, y = y + 1, width = 1, height = 1, value = cfg.fade_dur},
                check_combinar(cfg, y + 2),
            }
        end,
    },
    {
        nombre = "2. Movement (Cascada)",
        construir = function(cfg, y)
            return {
                lbl(0, y, 6, "Cada sílaba entra en cascada desde la dirección elegida y se queda en pantalla."),
                lbl(0, y + 1, 2, "  Dirección:"),
                {class = "dropdown", name = "dir_raw", x = 2, y = y + 1, width = 4, height = 1,
                 items = dir_labels, value = dir_labels[cfg.dir_idx] or dir_labels[1]},
                lbl(0, y + 2, 2, "  Velocidad (ms):"),
                {class = "intedit", name = "duracion",  x = 2, y = y + 2, width = 1, height = 1, value = cfg.duracion},
                lbl(3, y + 2, 2, "  Distancia (px):"),
                {class = "intedit", name = "distancia", x = 5, y = y + 2, width = 1, height = 1, value = cfg.distancia},
            }
        end,
    },
    {
        nombre = "3. Karaoke Fluido (Aparecer progresivo)",
        construir = function(cfg, y)
            return {
                lbl(0, y, 6, "Cada sílaba aparece de forma progresiva mientras se canta."),
                lbl(0, y + 1, 2, "  Duración (ms):"),
                {class = "intedit",  name = "fluido_dur",  x = 2, y = y + 1, width = 1, height = 1, value = cfg.fluido_dur},
                {class = "checkbox", name = "fluido_prim", x = 0, y = y + 2, width = 6, height = 1,
                 label = "Animar solo color primario (mantiene color secundario, borde y sombra)", value = cfg.fluido_prim},
                check_combinar(cfg, y + 3),
            }
        end,
    },
    {
        nombre = "4. Karaoke Reverso (Desaparecer total)",
        construir = function(cfg, y)
            return {
                lbl(0, y, 6, "Cada sílaba desaparece de forma progresiva mientras se canta."),
                lbl(0, y + 1, 2, "  Duración (ms):"),
                {class = "intedit", name = "rev_dur", x = 2, y = y + 1, width = 1, height = 1, value = cfg.rev_dur},
                check_combinar(cfg, y + 2),
            }
        end,
    },
    {
        nombre = "5. Karaoke Destacado",
        construir = function(cfg, y)
            return {
                lbl(0, y, 6, "La sílaba que se canta se resalta con subrayado (y opcionalmente con otro tamaño)."),
                {class = "checkbox", name = "dest_tam", x = 0, y = y + 1, width = 4, height = 1,
                 label = "Tamaño de la sílaba resaltada (fs):", value = cfg.dest_tam},
                {class = "intedit",  name = "dest_fs",  x = 4, y = y + 1, width = 2, height = 1, value = cfg.dest_fs},
                {class = "checkbox", name = "dest_completa", x = 0, y = y + 2, width = 6, height = 1,
                 label = "Línea completa con las palabras resaltadas (desactivado: aparece como karaoke)", value = cfg.dest_completa},
                check_combinar_fade(cfg, y + 3),
            }
        end,
    },
    {
        nombre = "6. Karaoke Central",
        construir = function(cfg, y)
            return {
                lbl(0, y, 2, "  Variación:"),
                {class = "dropdown", name = "central_var_raw", x = 2, y = y, width = 4, height = 1,
                 items = central_vars, value = central_vars[cfg.central_var] or central_vars[1]},
                lbl(0, y + 1, 2, "  Escala mayor (%):"),
                {class = "intedit", name = "central_big",   x = 2, y = y + 1, width = 1, height = 1, value = cfg.central_big},
                lbl(3, y + 1, 2, "  Escala menor (%):"),
                {class = "intedit", name = "central_small", x = 5, y = y + 1, width = 1, height = 1, value = cfg.central_small},
                lbl(0, y + 2, 2, "  Duración zoom (ms):"),
                {class = "intedit", name = "central_zoom_dur", x = 2, y = y + 2, width = 1, height = 1, value = cfg.central_zoom_dur},
                lbl(3, y + 2, 3, "  Brillo: Glow y fade de la página 1"),
                {class = "checkbox", name = "central_completa", x = 0, y = y + 3, width = 6, height = 1,
                 label = "Línea completa visible (desactivado: va apareciendo desde el centro)", value = cfg.central_completa},
            }
        end,
    },
}

local nombres_paginas = {}
for i, p in ipairs(paginas) do nombres_paginas[i] = p.nombre end

local function construir_ventana(cfg, pagina)
    local v = {}
    local function add(c) v[#v + 1] = c end

    -- Cabecera
    add(lbl(0, 0, 6, "◈ KARAOKE FX SUITE ◈"))
    add(lbl(0, 1, 6, SEP))

    -- Contenido de la página (alto fijo, relleno con filas vacías)
    local y0 = 2
    local contenido = paginas[pagina].construir(cfg, y0)
    local usadas = 0
    for _, c in ipairs(contenido) do
        add(c)
        usadas = math.max(usadas, c.y - y0 + 1)
    end
    for r = usadas, FILAS_PAGINA - 1 do add(lbl(0, y0 + r, 6, " ")) end

    -- Opciones generales (comunes a todos los efectos)
    local y = y0 + FILAS_PAGINA
    add(lbl(0, y, 6, SEP))
    add({class = "checkbox", name = "guardar",       x = 0, y = y + 1, width = 3, height = 1, label = "💾 Recordar últimos valores", value = cfg.guardar})
    add({class = "checkbox", name = "preserve_grad", x = 3, y = y + 1, width = 3, height = 1, label = "🌈 Preservar Gradientes (Ignora estilo base)", value = cfg.preserve_grad})
    add(lbl(0, y + 2, 2, "Posición X (auto):"))
    add({class = "intedit", name = "pos_x", x = 2, y = y + 2, width = 1, height = 1, value = cfg.pos_x})
    add(lbl(3, y + 2, 2, "Posición Y (auto):"))
    add({class = "intedit", name = "pos_y", x = 5, y = y + 2, width = 1, height = 1, value = cfg.pos_y})

    -- Selector de efecto (abajo)
    add(lbl(0, y + 3, 6, SEP))
    add(lbl(0, y + 4, 3, "Efecto a aplicar:"))
    add({class = "dropdown", name = "efecto_sel", x = 3, y = y + 4, width = 3, height = 1,
         items = nombres_paginas, value = nombres_paginas[pagina]})
    return v
end

-- Vuelca lo capturado en la ventana a cfg y devuelve el índice del efecto elegido en el selector
local function fusionar_resultado(cfg, res)
    for k, v in pairs(res) do cfg[k] = v end

    if res.dir_raw then
        for i, etiqueta in ipairs(dir_labels) do
            if etiqueta == res.dir_raw then cfg.dir_idx = i break end
        end
        cfg.dir_raw = nil
    end

    if res.central_var_raw then
        for i, nombre in ipairs(central_vars) do
            if nombre == res.central_var_raw then cfg.central_var = i break end
        end
        cfg.central_var_raw = nil
    end

    local elegido
    for i, nombre in ipairs(nombres_paginas) do
        if nombre == res.efecto_sel then elegido = i break end
    end
    cfg.efecto_sel = nil
    return elegido
end

local function abrir_karaokefx(subs, sel, act)
    local styles = {}
    local playres_x, playres_y = 1280, 720
    for i = 1, #subs do
        local l = subs[i]
        if l.class == "info" then
            if l.key == "PlayResX" then playres_x = tonumber(l.value) or playres_x end
            if l.key == "PlayResY" then playres_y = tonumber(l.value) or playres_y end
        elseif l.class == "style" then
            styles[l.name] = l
        elseif l.class == "dialogue" then break end
    end

    local cfg = cargar_config()
    local def_x, def_y = cfg.pos_x, cfg.pos_y
    if #sel > 0 then
        def_x, def_y = get_line_pos(subs[sel[1]], styles, playres_x, playres_y)
        cfg.pos_x = math.floor(def_x)
        cfg.pos_y = math.floor(def_y)
    end

    local pagina = math.min(math.max(math.floor(cfg.efecto_idx or 1), 1), #paginas)

    -- Bucle de páginas: "Cambiar efecto" (o aplicar con otro efecto en el selector) solo
    -- cambia de página; "Aplicar" con la página ya elegida ejecuta el proceso.
    while true do
        local btn, res = aegisub.dialog.display(
            construir_ventana(cfg, pagina),
            {BTN_APLICAR, BTN_CAMBIAR, BTN_CANCELAR},
            {ok = BTN_APLICAR, cancel = BTN_CANCELAR})

        if btn ~= BTN_APLICAR and btn ~= BTN_CAMBIAR then return end -- Cancelar o cerrar con la X

        local elegido = fusionar_resultado(cfg, res) or pagina
        if btn == BTN_CAMBIAR or elegido ~= pagina then
            pagina = elegido
        else
            break
        end
    end

    cfg.efecto_idx = pagina
    guardar_config(cfg)

    -- Banderas que usa el motor de procesamiento (un efecto por página).
    -- Combinaciones: Movement con las páginas 1, 3 y 4 (casilla comb_mov);
    -- Color Fade con Destacado, página 5 (casilla comb_fade). Cada casilla solo vale en sus páginas.
    local final = {}
    for k, v in pairs(cfg) do final[k] = v end
    final.color_fade = (pagina == 1) or (pagina == 5 and cfg.comb_fade == true)
    final.movement   = (pagina == 2) or (pagina < 5 and cfg.comb_mov == true)
    final.fluido     = (pagina == 3)
    final.reverse    = (pagina == 4)
    final.destacado  = (pagina == 5)
    final.central    = (pagina == 6) -- usa el brillo de la página 1 por su cuenta; no se combina con otros efectos

    local is_manual = (final.pos_x ~= math.floor(def_x)) or (final.pos_y ~= math.floor(def_y))
    procesar_karaokefx(subs, sel, final, styles, playres_x, playres_y, is_manual)
end

aegisub.register_macro("Karaoke FX", "Color Fade, Movement, Fluido, Reverso, Destacado y Central en una sola ventana", abrir_karaokefx)
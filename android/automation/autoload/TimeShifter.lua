script_name = "Shift Timer Custom"
script_description = "Desplaza las líneas seleccionadas a un nuevo tiempo de inicio, manteniendo la separación relativa entre ellas."
script_author = "Soniz"
script_version = "1.0"

-- Función para convertir milisegundos a formato de tiempo de Aegisub (H:MM:SS.cc)
local function ms2time(ms)
    local h = math.floor(ms / 3600000)
    ms = ms % 3600000
    local m = math.floor(ms / 60000)
    ms = ms % 60000
    local s = math.floor(ms / 1000)
    local cs = math.floor((ms % 1000) / 10)
    return string.format("%d:%02d:%02d.%02d", h, m, s, cs)
end

-- Función para convertir formato de tiempo de Aegisub (H:MM:SS.cc) a milisegundos
local function time2ms(time_string)
    local h, m, s, ms_str = string.match(time_string, "^(%d+):(%d%d):(%d%d)%.(%d+)$")
    if h and m and s and ms_str then
        local ms = tonumber(ms_str)
        -- Si el usuario ingresa centisegundos (2 dígitos, formato por defecto de Aegisub), lo pasamos a ms
        if string.len(ms_str) == 2 then ms = ms * 10 end 
        return (tonumber(h) * 3600 + tonumber(m) * 60 + tonumber(s)) * 1000 + ms
    end
    return nil
end

function shift_timer(subs, sel)
    -- Verificar que haya líneas seleccionadas
    if #sel == 0 then
        aegisub.cancel()
    end

    -- Obtener la primera línea seleccionada para usarla como base
    local first_line = subs[sel[1]]
    local original_start_ms = first_line.start_time

    -- Configurar la ventana de diálogo
    local dialog = {
        {
            class = "label",
            x = 0, y = 0, width = 1, height = 1,
            label = "Nuevo tiempo de inicio para el bloque (H:MM:SS.cc):"
        },
        {
            class = "edit",
            name = "new_time",
            x = 1, y = 0, width = 1, height = 1,
            text = ms2time(original_start_ms) -- Pre-llenar con el tiempo actual
        }
    }

    local btn, res = aegisub.dialog.display(dialog, {"Aplicar", "Cancelar"})

    if btn == "Aplicar" then
        local new_start_ms = time2ms(res.new_time)
        
        -- Validar que el formato introducido sea correcto
        if not new_start_ms then
            aegisub.debug.out("Error: El formato de tiempo es inválido. Por favor, usa el formato H:MM:SS.cc (ejemplo: 0:01:23.45)")
            aegisub.cancel()
        end

        -- Calcular la diferencia de tiempo (puede ser positiva o negativa)
        local time_diff = new_start_ms - original_start_ms

        -- Aplicar la diferencia a todas las líneas seleccionadas
        for i = 1, #sel do
            local line = subs[sel[i]]
            line.start_time = line.start_time + time_diff
            line.end_time = line.end_time + time_diff
            subs[sel[i]] = line
        end

        -- Crear un punto de restauración para que el usuario pueda usar Ctrl+Z si se equivoca
        aegisub.set_undo_point("Shift Timer")
    end
end

-- Registrar la macro en Aegisub
aegisub.register_macro("Shift Timer Custom", "Cambia el tiempo de inicio de un bloque de subtítulos manteniendo sus intervalos.", shift_timer)
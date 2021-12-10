-- xxd -i hello.lua > hello_lua.c
-- FIXME need to teach cmake how to do this

for x = 1, 8 do
   for y = 1, 8 do
      lcd_buffer[x + 8*y] = y*8 + x+y
   end
end

for x = 10, 200, 12 do
   draw_lcd_buffer(x, x, 8, 8, 8*8*2)
end

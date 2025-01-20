let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/hybrid_piano/teensy_fwd
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +351 src/key.cpp
badd +1 src/ui.cpp
badd +698 src/magnets.cpp
badd +9 src/config.cpp
badd +22 src/sampler2.cpp
badd +8 src/unit.h
badd +382 src/utils.cpp
badd +1 src/keyboard.cpp
badd +1 src/config_mgmt.cpp
badd +431 src/calibrate.cpp
badd +45 src/main.cpp
badd +12 src/newmain.cpp
badd +5 src/commands.cpp
badd +122 src/vector.cpp
badd +258 /Library/Developer/CommandLineTools/usr/lib/clang/16/include/stdint.h
badd +27 platformio.ini
badd +0 ~/.config/nvim/coc-settings.json
badd +1 newmain.cpp
badd +56 ~/.platformio/packages/framework-arduinoteensy/cores/teensy4/Stream.h
badd +1 src/types.cpp
badd +188 ~/.platformio/packages/framework-arduinoteensy/cores/teensy4/usb_serial.h
badd +1 src/sampler_generated.h
badd +1 include/README
badd +1 include/Array.h
badd +1 src/result.h
badd +1428 include/flatbuffers/flatbuffer_builder.h
badd +27 ~/hybrid_piano/sampler_generated.h
badd +2 ~/hybrid_piano/.vim/coc-settings.json
badd +7 sampler.fbs
badd +53 ~/.config/nvim/vimrc.vim
argglobal
%argdel
set stal=2
tabnew +setlocal\ bufhidden=wipe
tabnew +setlocal\ bufhidden=wipe
tabnew +setlocal\ bufhidden=wipe
tabrewind
edit src/commands.cpp
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
wincmd _ | wincmd |
split
1wincmd k
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 31 + 127) / 255)
exe 'vert 2resize ' . ((&columns * 111 + 127) / 255)
exe '3resize ' . ((&lines * 36 + 37) / 74)
exe 'vert 3resize ' . ((&columns * 111 + 127) / 255)
exe '4resize ' . ((&lines * 35 + 37) / 74)
exe 'vert 4resize ' . ((&columns * 111 + 127) / 255)
argglobal
enew
file NERD_tree_tab_4
balt src/sampler2.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal nofen
wincmd w
argglobal
balt include/flatbuffers/flatbuffer_builder.h
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 5 - ((4 * winheight(0) + 36) / 72)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 5
normal! 023|
wincmd w
argglobal
if bufexists(fnamemodify("~/hybrid_piano/sampler_generated.h", ":p")) | buffer ~/hybrid_piano/sampler_generated.h | else | edit ~/hybrid_piano/sampler_generated.h | endif
if &buftype ==# 'terminal'
  silent file ~/hybrid_piano/sampler_generated.h
endif
balt src/commands.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 32 - ((8 * winheight(0) + 18) / 36)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 32
normal! 04|
wincmd w
argglobal
if bufexists(fnamemodify("~/.config/nvim/coc-settings.json", ":p")) | buffer ~/.config/nvim/coc-settings.json | else | edit ~/.config/nvim/coc-settings.json | endif
if &buftype ==# 'terminal'
  silent file ~/.config/nvim/coc-settings.json
endif
balt ~/hybrid_piano/.vim/coc-settings.json
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 13 - ((12 * winheight(0) + 17) / 35)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 13
normal! 020|
wincmd w
exe 'vert 1resize ' . ((&columns * 31 + 127) / 255)
exe 'vert 2resize ' . ((&columns * 111 + 127) / 255)
exe '3resize ' . ((&lines * 36 + 37) / 74)
exe 'vert 3resize ' . ((&columns * 111 + 127) / 255)
exe '4resize ' . ((&lines * 35 + 37) / 74)
exe 'vert 4resize ' . ((&columns * 111 + 127) / 255)
tabnext
argglobal
enew
file \[Plugins]
balt ~/.config/nvim/vimrc.vim
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
tabnext
edit sampler.fbs
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 31 + 127) / 255)
exe 'vert 2resize ' . ((&columns * 111 + 127) / 255)
exe 'vert 3resize ' . ((&columns * 111 + 127) / 255)
argglobal
enew
file NERD_tree_tab_5
balt sampler.fbs
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal nofen
wincmd w
argglobal
balt ~/hybrid_piano/sampler_generated.h
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 7 - ((6 * winheight(0) + 36) / 72)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 7
normal! 08|
wincmd w
argglobal
if bufexists(fnamemodify("~/.config/nvim/vimrc.vim", ":p")) | buffer ~/.config/nvim/vimrc.vim | else | edit ~/.config/nvim/vimrc.vim | endif
if &buftype ==# 'terminal'
  silent file ~/.config/nvim/vimrc.vim
endif
balt sampler.fbs
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 53 - ((52 * winheight(0) + 36) / 72)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 53
normal! 028|
wincmd w
exe 'vert 1resize ' . ((&columns * 31 + 127) / 255)
exe 'vert 2resize ' . ((&columns * 111 + 127) / 255)
exe 'vert 3resize ' . ((&columns * 111 + 127) / 255)
tabnext
edit ~/.config/nvim/coc-settings.json
argglobal
balt src/newmain.cpp
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 23 - ((22 * winheight(0) + 36) / 72)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 23
normal! 061|
tabnext 2
set stal=1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :

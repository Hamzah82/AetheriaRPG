# Aetheria: Bayangan Terakhir

RPG 2D berbahasa Indonesia yang ditulis dengan C++17. Jendela memakai X11 di
Linux dan Win32/GDI di Windows. Seluruh grafis, font, efek, musik, dan suara
dalam game dibuat secara prosedural; satu-satunya aset adalah ikon aplikasi,
tanpa dependensi SFML/SDL.

Pemain menjelajahi Desa Elmwood, Padang Aetheria, Gua Bayangan, dan Reruntuhan
Fajar untuk merebut kembali Batu Matahari dari Warden Bayangan, lalu menghadapi
Penjaga Fajar di reruntuhan kuno.

## Fitur

- Empat peta yang saling terhubung lewat portal, lengkap dengan NPC, peti harta,
  papan petunjuk, bangunan, dan ruang bos.
- Pertarungan turn-based: Serang, Sihir (Bola Api, Petir, Pulihkan), Item, dan
  Kabur. Monster Slime, Kelelawar, Serigala, dan Rangka muncul sesuai wilayah.
- Dua bos: **Warden Bayangan** dan **Penjaga Fajar**, masing-masing dengan
  dialog dan pertarungan khusus.
- Misi utama serta misi sampingan **Ramuan untuk Ruslan** (3 Ramuan untuk 120
  emas dan satu Eliksir).
- Tiga slot simpan manual ditambah **autosave** di titik-titik cerita. Layar
  judul memuat autosave, Slot 1-3, atau kembali tanpa memuat apa pun.
- Jurnal berisi tujuan misi utama/sampingan dan daftar peta yang sudah
  dikunjungi, ditambah minimap langsung di HUD dan halaman jurnal.
- Perlengkapan tier-3 di Reruntuhan Fajar: **Pedang Fajar** dan **Perisai
  Fajar**.
- Penampilan hero mengikuti perlengkapan yang dipakai: pedang kayu, perunggu,
  besi, dan Fajar tampil di tangan, sedangkan armor kain, kulit, rantai, dan
  pelat Fajar mengubah warna tubuh, pelat bahu, serta perisai.
- Musik chiptune dan efek suara prosedural, dengan **crossfade** saat lagu
  berganti dan jeda otomatis saat jendela kehilangan fokus.
- Halaman **Menu > Audio** untuk mengatur volume musik, volume efek, dan
  bisukan; pengaturan disimpan ke `settings.cfg`.
- Renderer software 320×240 yang diperbesar ke jendela X11 atau Win32 (bawaan
  3× menjadi 960×720), sehingga tampilan tetap tajam dan ringan.
- Mode layar penuh dengan penskalaan integer dan bilah hitam; tekan `F11` atau
  `Alt+Enter` untuk beralih. Pilihan layar penuh ikut tersimpan.
- Ikon jendela prosedural di kedua platform, launcher desktop SVG di Linux, dan
  ikon `.exe` serta shortcut Start Menu di Windows, dipasang otomatis oleh
  `install.sh`.
- Mode `--headless` dan `--screenshot` untuk pengujian tanpa jendela, serta
  workflow CI GitHub Actions.

## Kebutuhan

### Linux

- Sesi X11 (misalnya Linux Mint, Ubuntu, Debian).
- Compiler C++17 (`g++` atau `clang++`).
- `make`.
- Git (`git`) untuk instalasi cepat dan menu **Perbarui Game**.
- Header pengembangan X11 (`libX11`).
- Opsional: `libasound.so.2` untuk musik dan efek suara. Tanpa pustaka atau
  perangkat audio, game tetap berjalan dalam mode senyap.

Di Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential libx11-dev git
```

Tidak diperlukan pustaka grafis lain. Header ALSA **tidak** dibutuhkan karena
`libasound.so.2` dimuat saat runtime; cukup paket runtime `libasound2` yang
biasanya sudah terpasang.

### Windows

- Windows 10 atau 11 (64-bit).
- Git for Windows (menyertakan **Git Bash**), atau MSYS2.
- Compiler MinGW-w64 dan `make` dari MSYS2. `install.sh` dapat memasang MSYS2
  dan toolchain-nya otomatis lewat `winget`.
- Git untuk instalasi cepat dan menu **Perbarui Game**.

Cara termudah: pasang **Git for Windows** dan **MSYS2**, lalu buka
**MSYS2 MinGW 64-bit** dan jalankan:

```bash
pacman -S --needed mingw-w64-x86_64-gcc make git
```

Alternatifnya, buka **Git Bash** di folder proyek dan jalankan `./install.sh`;
skrip akan menawarkan pemasangan MSYS2 lewat `winget` bila compiler belum ada.
Game memakai `winmm` untuk audio dan tidak membutuhkan DLL runtime MinGW karena
biner ditautkan secara statis.

## Instalasi Cepat

Cara termudah untuk memeriksa dependensi, membangun, dan memasang game:

```bash
chmod +x install.sh
./install.sh
```

Skrip akan:

1. Memeriksa compiler C++17, `make`, `git`, serta header X11 (Linux) atau
   compiler MinGW-w64 dan pustaka Win32 (Windows).
2. Menawarkan pemasangan paket yang kurang melalui manajer paket distro
   (`apt`, `dnf`, `pacman`, `zypper`, atau `apk`) atau, di Windows, melalui
   MSYS2/pacman dan `winget`.
3. Mengkloning sumber dari GitHub ke `~/.aetheria/source` bila belum ada, atau
   memperbaruinya dengan `git pull --ff-only` saat pemasangan ulang.
4. Membangun game dari salinan sumber itu dan menjalankan uji asap `--headless`.
5. Memasang biner dan peluncur. Di Linux juga dipasang launcher desktop serta
   ikon SVG; di Windows dipasang `Aetheria.bat` dan shortcut Start Menu, dan
   ikon tertanam di `rpgcpp.exe`.

Semua kebutuhan game disatukan dalam satu folder:

```text
~/.aetheria/
  rpgcpp                 biner game (Windows: rpgcpp.exe)
  aetheria               peluncur Linux (Windows: Aetheria.bat)
  settings.cfg           pengaturan audio dan layar penuh
  savegame.dat           Slot 1 (dibuat saat menyimpan)
  savegame-2.dat         Slot 2
  savegame-3.dat         Slot 3
  savegame-auto.dat      autosave
  source/                salinan Git sumber game untuk menu Perbarui Game
```

Di Windows folder ini berada di `C:\Users\<nama>\.aetheria`. Ikon aplikasi Linux
dipasang ke `~/.local/share/icons/hicolor/scalable/apps/aetheria.svg`, dan
launcher desktop memakai `Icon=aetheria` sehingga tema ikon dapat menemukannya.
Di Windows, `rpgcpp.exe` membawa ikonnya sendiri dan shortcut Start Menu memakai
ikon tersebut.

Opsi yang tersedia:

| Opsi | Fungsi |
| --- | --- |
| `--dir DIR` | Folder data game (bawaan `~/.aetheria`). |
| `--prefix DIR` | Alias lama untuk `--dir`. |
| `--repo URL` | Repositori sumber (bawaan `https://github.com/Hamzah82/AetheriaRPG.git`). |
| `--jobs N` | Jumlah pekerjaan build (bawaan jumlah CPU). |
| `--build-only` | Hanya membangun checkout saat ini, tidak memasang. |
| `--clean` | Hapus hasil build sebelum membangun. |
| `--no-deps` | Jangan pasang dependensi otomatis. |
| `-y`, `--yes` | Jawab ya saat diminta memasang paket. |
| `--uninstall` | Hapus biner, peluncur, dan launcher/shortcut; simpanan dan sumber dipertahankan. |
| `-h`, `--help` | Tampilkan bantuan. |

Contoh:

```bash
./install.sh --build-only
./install.sh --dir ~/games/aetheria
./install.sh --repo https://github.com/pengguna/AetheriaRPG.git
./install.sh --uninstall
```

Jalankan game lewat menu aplikasi, shortcut Start Menu, `~/.aetheria/aetheria`
(Linux), atau `~/.aetheria/Aetheria.bat` (Windows). Skrip uninstall tidak
menghapus berkas `savegame*.dat`, `settings.cfg`, maupun salinan sumber di
`~/.aetheria/source`; folder game hanya dihapus bila sudah kosong. Instalasi lama
di `~/.local/bin/rpgcpp` dibersihkan otomatis di Linux. Berkas simpanan lama yang
berada di folder lain perlu dipindahkan manual ke `~/.aetheria/savegame.dat`.

## Perbarui Game dari Judul

Layar judul memiliki empat pilihan: **Mulai Baru**, **Lanjutkan**,
**Perbarui Game**, dan **Keluar**. **Mulai Baru** meminta konfirmasi bila sudah
ada simpanan (pilihan bawaan **Batal**), sedangkan **Lanjutkan** membuka halaman
muat berisi **Otomatis**, **Slot 1-3**, dan **Kembali**. Memilih **Perbarui
Game** akan:

1. Menjalankan `git pull --ff-only` pada salinan sumber
   `~/.aetheria/source`.
2. Membangun ulang game dengan `make -jN` memakai compiler yang tersedia.
3. Menguji biner baru dengan `--headless --frames 30`.
4. Memasang biner baru ke lokasi instalasi bila game dijalankan dari luar
   checkout (misalnya `~/.aetheria/rpgcpp` atau `rpgcpp.exe`).
5. Menampilkan **Mulai Ulang** setelah berhasil; tekan `Z` untuk menjalankan
   versi baru.

Pembaruan berjalan di latar belakang sehingga layar judul tetap responsif dan
menampilkan tahap yang sedang dikerjakan. Menu ini membutuhkan `git` serta
checkout sumber yang bersih; perubahan lokal yang belum di-commit akan
menghentikan pembaruan. Jika build atau uji gagal, biner lama dipulihkan
otomatis.

`install.sh` mengkloning repositori ke `~/.aetheria/source`, sehingga game yang
terpasang dapat memperbarui dirinya tanpa bergantung pada folder proyek asal.
Pemasangan ulang menjalankan `git pull --ff-only` lalu membangun dari salinan
yang sama. Untuk checkout di lokasi lain, set
`AETHERIA_REPO=/path/ke/sumber` sebelum menjalankan game. Pada mode `--headless`
dan `--screenshot`, menu pembaruan tidak menjalankan perintah apa pun.

Di Windows, menu **Perbarui Game** memerlukan `git` dan `make` di `PATH`
(misalnya dengan membuka **MSYS2 MinGW 64-bit**), dan `AETHERIA_REPO` menerima
path Windows. Bila toolchain tidak tersedia dari shortcut, jalankan
`./install.sh` lagi; skrip itu menarik sumber terbaru lalu membangun ulang dan
memasang binernya.

## Build dan Menjalankan

Linux:

```bash
make -j4
./rpgcpp
```

Windows (dari **MSYS2 MinGW 64-bit**, **Git Bash** dengan toolchain MinGW-w64 di
`PATH`, atau silang dari Linux):

```bash
make -j4
./rpgcpp.exe
```

Makefile mendeteksi Windows dari compiler (`-dumpmachine` berisi `mingw`) atau
variabel `OS=Windows_NT`, lalu memakai akhiran `.exe`, subsistem GUI, penautan
statis, dan pustaka `gdi32`, `user32`, serta `winmm`. Set `WINDOWS=1` untuk
memaksa mode Windows dan `WINDRES=...` bila `windres` tidak ada di `PATH`.
Kompilasi silang dari Linux misalnya:

```bash
make CXX=x86_64-w64-mingw32-g++ WINDRES=x86_64-w64-mingw32-windres
```

Perintah lain yang tersedia:

```bash
make run                 # build lalu jalankan
make test                # jalankan rangkaian tes regresi
make clean               # hapus hasil build
```

## Kontrol

| Tombol | Fungsi |
| --- | --- |
| Panah / `W` `A` `S` `D` | Bergerak |
| `Shift` | Berlari |
| `Z` / `Enter` / `Spasi` | Konfirmasi, bicara, buka peti, pilih menu |
| `X` / `Esc` / `Backspace` | Batal, kembali, tutup menu |
| `E` / `Tab` | Buka menu utama saat menjelajah |
| `F11` / `Alt+Enter` | Beralih mode layar penuh |

Saat dialog, tombol konfirmasi atau batal akan mempercepat teks lalu melanjutkan
ke halaman berikutnya. Di dalam pertarungan, gunakan tombol arah untuk memilih
perintah dan `X` untuk kembali dari submenu Sihir/Item. Di halaman **Audio**,
tombol kiri/kanan mengubah volume atau pilihan, `Z` memilih, dan `X` kembali.
Menu menjelajah memakai tombol arah untuk berpindah baris; **Jurnal** menampilkan
misi dan peta, sedangkan **Simpan** meminta konfirmasi bila slot sudah terisi.

## Alur Permainan

1. Mulai permainan baru dari judul. Intro narator akan memperkenalkan Lembah
   Aetheria.
2. Temui **Tetua Ardan** di Desa Elmwood untuk menerima quest. Autosave ditulis
   saat quest diterima.
3. Opsional: temui **Petani Ruslan** di desa dan bawa 3 Ramuan untuk misi
   sampingan.
4. Keluar desa melalui jalan di sisi selatan menuju **Padang Aetheria**.
5. Monster berkeliaran di rumput tinggi Padang Aetheria, lantai Gua Bayangan,
   dan lantai Reruntuhan Fajar. Sentuh monster untuk memulai pertarungan;
   tidak ada pertarungan acak saat melangkah. Peti harta tersebar di beberapa
   lokasi.
6. Masuk ke **Gua Bayangan** di timur laut padang (melewati jembatan tua).
7. Kalahkan **Warden Bayangan** di ruang paling dalam untuk mendapatkan
   **Batu Matahari**. Autosave ditulis setelah kemenangan.
8. Dengan Batu Matahari, portal di balik tembok timur laut Padang Aetheria
   terbuka menuju **Reruntuhan Fajar**. Kalahkan **Penjaga Fajar**, ambil
   **Pedang Fajar** dan **Perisai Fajar**, lalu selesaikan misi sampingan bila
   belum.
9. Kembali ke Tetua Ardan untuk menyelesaikan permainan dan melihat ending.
   Setelah ending, **Lanjutkan** dapat memuat autosave untuk menjelajah sisa
   reruntuhan.

Desa Elmwood aman dari monster. Penginapan memulihkan HP dan MP penuh dengan
biaya 10 emas (hanya jika pemain cukup lelah dan memiliki emas).

## Pertarungan

- Monster terlihat berkeliaran di peta. Sentuh mereka untuk memulai
  pertarungan; setelah kalah atau kabur, monster menghilang sementara lalu
  muncul kembali di sekitar sarangnya.
- **Serang** memakai senjata yang sedang dipakai dan dapat menghasilkan serangan
  kritis.
- **Sihir** memakai MP: Bola Api (5 MP), Petir (7 MP), dan Pulihkan (5 MP).
- **Item** memakai Ramuan (+40 HP) atau Eliksir (+25 MP).
- **Kabur** memiliki peluang 70%, tetapi tidak bisa dipakai melawan boss.
- Kemenangan memberi XP dan emas. Naik level memulihkan HP/MP penuh dan
  meningkatkan statistik.
- Jika HP mencapai 0, pemain dapat bangkit di Elmwood dengan kehilangan 20% emas
  atau kembali ke judul.

## Toko dan Item

Toko Rilla di Desa Elmwood menjual:

| Item | Harga | Efek |
| --- | --- | --- |
| Ramuan | 12 emas | Memulihkan 40 HP |
| Eliksir | 30 emas | Memulihkan 25 MP |
| Pedang Perunggu | 60 emas | Senjata serangan +4 |
| Armor Kulit | 50 emas | Pertahanan +3 |
| Pedang Besi | 140 emas | Senjata serangan +8 |
| Baju Rantai | 130 emas | Pertahanan +7 |

Item yang lebih kuat hanya dapat dibeli sekali dan tidak menurunkan kualitas
perlengkapan yang sudah dimiliki.

Di peti Reruntuhan Fajar terdapat perlengkapan tier-3: **Pedang Fajar** (+13
serangan) dan **Perisai Fajar** (+11 pertahanan). Keduanya tidak dijual di toko;
peti cadangan dapat dijual seharga 200 emas bila perlengkapan itu sudah dipakai.

## Musik dan Efek Suara

Seluruh audio disintesis saat program berjalan dengan gelombang kotak, segitiga,
sinus, gergaji, dan noise; tidak ada berkas musik atau suara yang dimuat.

- Tujuh lagu latar: judul, desa, padang, gua, pertarungan, boss, dan ending.
  Lagu berganti otomatis mengikuti state permainan dan peta, dengan crossfade
  singkat agar peralihan tidak kasar.
- Efek suara untuk langkah, menu, peti, koin, ramuan, serangan, serangan kritis,
  luka, sihir api/petir, pulih, kabur, menang, kalah, naik level, simpan, portal,
  dan auman boss.
- **Menu > Audio** menyediakan pengaturan volume musik, volume efek suara, dan
  bisukan. Pengaturan berlaku langsung dan disimpan ke `settings.cfg` saat
  berubah atau saat game ditutup.
- Saat jendela kehilangan fokus, simulasi dan audio dijeda; ketika fokus
  kembali, permainan berlanjut tanpa lonjakan waktu.
- Audio bersifat opsional. Jika `libasound.so.2` atau perangkat suara tidak
  tersedia, game menampilkan pesan dan berjalan tanpa suara.
- Gunakan `--no-audio` untuk mematikan audio sepenuhnya sejak awal.

## Simpan dan Muat

- Menu saat menjelajah memiliki enam pilihan: **Item**, **Status**, **Jurnal**,
  **Simpan**, **Audio**, dan **Tutup**.
- Halaman **Simpan** menampilkan autosave dan tiga slot manual. Menimpa slot
  yang sudah terisi memerlukan konfirmasi kedua (`Z` dua kali).
- Pada instalasi, berkas simpanan berada di `~/.aetheria/`:
  `savegame.dat` (Slot 1), `savegame-2.dat` (Slot 2), `savegame-3.dat` (Slot 3),
  dan `savegame-auto.dat` (autosave).
- Autosave ditulis otomatis saat quest utama diterima, setelah Warden dan
  Penjaga Fajar dikalahkan, saat misi Petani Ruslan selesai, dan saat Batu
  Matahari diserahkan ke Tetua.
- **Lanjutkan** pada layar judul membuka halaman muat berisi **Otomatis**,
  **Slot 1-3**, dan **Kembali**; Slot 1 dipilih secara bawaan.
- **Mulai Baru** meminta konfirmasi bila sudah ada simpanan. Pilihan bawaan
  adalah **Batal** agar simpanan tidak terhapus tanpa sengaja.
- Saat dijalankan langsung dari checkout sumber, berkas simpanan berada di
  direktori kerja.
- Lokasi dasar berkas dapat diubah dengan `--save PATH`; slot mengikuti awalan
  yang sama.
- Berkas simpanan dikodekan dan dilengkapi checksum sebelum ditulis, lalu
  diganti secara atomik melalui berkas sementara. Suntingan manual pada berkas
  akan ditolak saat dimuat. Ini adalah penyamaran data untuk mencegah kecurangan
  sehari-hari, bukan enkripsi kuat, dan simpanan baru tidak dapat dibuka oleh
  build lama. Simpanan lama dalam teks polos (versi 1 dan 2) tetap dapat dimuat
  dan akan ditulis ulang dalam format terkode pada penyimpanan berikutnya.
- Pengaturan audio dan layar penuh disimpan terpisah di `settings.cfg`, sehingga
  tidak ikut berubah saat memuat slot lain atau memulai permainan baru. Gunakan
  `--settings PATH` untuk memilih lokasi lain.

## Opsi Command Line

```text
./rpgcpp [opsi]

  --scale N        perbesaran jendela (1-6, bawaan 3)
  --seed N         benih acak
  --save PATH      lokasi berkas simpanan
  --settings PATH  lokasi berkas pengaturan
  --headless       uji jalan tanpa jendela
  --frames N       jumlah frame uji headless
  --no-audio       jalankan tanpa musik dan efek suara
  --fullscreen     mulai dalam mode layar penuh
  --screenshot DIR tulis tangkapan layar lalu keluar
  --help, -h       tampilkan bantuan
```

Contoh:

```bash
./rpgcpp --scale 4
./rpgcpp --no-audio
./rpgcpp --settings /tmp/aetheria.cfg
./rpgcpp --headless --frames 1200
./rpgcpp --screenshot /tmp/opencode/shots
```

Mode `--screenshot` memakai berkas simpanan terpisah (`PATH.shot` beserta slot
turunannya) agar tidak mengganggu simpanan permainan biasa. Hasilnya adalah 19
PNG 960×720 dari layar judul, desa, dialog, padang, gua, pertarungan, menu,
jurnal, simpan, audio, toko, game over, reruntuhan, bos Penjaga Fajar, dan
ending.

## Struktur Proyek

```text
src/core.h          tipe dasar, RNG, palet, tombol
src/gfx.h/.cpp      framebuffer, font bitmap, ekspor PNG
src/player.h/.cpp   statistik pemain, item, senjata, armor
src/world.h/.cpp    data peta, portal, NPC, peti, collision
src/art.h/.cpp      sprite dan tile prosedural
src/audio_synth.h/.cpp musik dan efek suara prosedural
src/audio.h/.cpp    pemutaran ALSA runtime, pencampuran, volume
src/battle.h/.cpp   logika dan tampilan pertarungan
src/game.h          antarmuka Game dan hook pengujian
src/game.cpp        state machine, dialog, quest, toko, simpan/muat
src/game_render.cpp tampilan judul, menu, jurnal, minimap, toko, status, ending
src/settings.h/.cpp pengaturan audio/layar penuh yang tersimpan
src/savegame.h/.cpp penamaan slot, metadata, dan ringkasan simpanan
src/updater.h/.cpp  git pull dan build ulang dari menu judul
src/platform.h/.cpp jendela X11, input, fokus, ikon, presentasi piksel
src/platform_win32.cpp jendela Win32/GDI, input, fokus, ikon, presentasi piksel
src/main.cpp        CLI, loop utama, headless, screenshot
assets/aetheria.svg ikon aplikasi untuk launcher desktop Linux
assets/aetheria.ico ikon aplikasi Windows (multi-ukuran)
assets/aetheria.rc  resource ikon yang ditanam ke rpgcpp.exe
tests/test_main.cpp rangkaian tes regresi
Makefile            target build, test, run, shots, clean
install.sh          kloning sumber, build, instalasi, uninstall
.github/workflows/ci.yml  build, tes, dan uji asap headless di GitHub Actions
```

## Pengujian

```bash
make test
```

Tes mencakup RNG dan matematika dasar, statistik pemain, leveling, konektivitas
peta (BFS) untuk empat peta, collision, portal dan segel Batu Matahari, peti,
dialog, toko, penginapan, seluruh alur pertarungan termasuk Penjaga Fajar, misi
Petani Ruslan, quest utama, simpan/muat (termasuk tiga slot, autosave, berkas
rusak, dan kompatibilitas versi 1), pengaturan audio/layar penuh, layar judul
beserta konfirmasi dan halaman muat, menu enam baris, jurnal, minimap, variasi
visual senjata/armor pada sprite hero, game over, ending, determinisme render,
ikon jendela, ekspor PNG, pemetaan warna X11, render
setiap state permainan, serta sintesis audio (durasi, puncak, loop mulus,
determinisme) dan pengaturan volume/bisukan/jeda tanpa perangkat suara. Tes tidak
pernah membuka perangkat audio sehingga tetap senyap dan deterministik.

Workflow `.github/workflows/ci.yml` menjalankan build, `make test`, uji asap
`--headless --frames 1200`, dan uji tangkapan layar pada setiap push ke `main`
serta pull request.

Uji asap tanpa jendela:

```bash
./rpgcpp --headless --frames 1200
```

Keluaran yang diharapkan berakhir dengan baris seperti:

```text
headless ok: frames=1200 state=1 map=0 hp=42 checksum=...
```

## Catatan Teknis

- Resolusi internal tetap 320×240; presentasi X11 dan Win32 memakai penskalaan
  nearest-neighbour agar piksel tetap tegas.
- Di Linux, warna piksel dipetakan ke mask merah/hijau/biru milik visual X11,
  sehingga tampilan benar pada layout RGB maupun BGR (termasuk kanal 10-bit).
  Di Windows, framebuffer disalin ke DIB 32-bit lalu diperbesar dengan
  `StretchBlt` mode `COLORONCOLOR` agar piksel tidak dihaluskan.
- Layar penuh memakai `_NET_WM_STATE_FULLSCREEN` di X11 dan jendela tanpa border
  di Win32. Framebuffer diperbesar dengan skala integer terbesar yang muat lalu
  dipusatkan; sisa ruang menjadi bilah hitam sehingga rasio 4:3 tidak berubah
  dan piksel tetap tegas.
- Logika permainan tidak bergantung pada backend jendela, sehingga dapat diuji
  secara headless dan deterministik dengan benih acak tetap.
- Monster penjelajah ditempatkan secara deterministik dari benih permainan,
  hanya di petak encounter (rumput tinggi atau lantai gua), dan tidak pernah
  menempati petak portal, NPC, atau peti. Posisi mereka tidak disimpan ke
  berkas simpanan; susunannya disegarkan saat permainan baru atau dimuat.
- Font 5×7 dan seluruh sprite digambar langsung ke framebuffer, tanpa memuat
  berkas gambar.
- Mesin audio memakai satu thread pencampuran dengan potongan 512 frame pada
  44100 Hz mono S16. Di Linux `libasound.so.2` dimuat melalui `dlopen`, sehingga
  build tidak memerlukan header ALSA; di Windows output memakai `waveOut` dari
  `winmm` dengan dua buffer bergantian. Tanpa pustaka atau perangkat audio, game
  tetap berjalan dalam mode senyap.
- Musik dan efek suara dibangkitkan secara deterministik dari not dan parameter
  tetap; hasilnya sama pada setiap mesin dan dapat diuji tanpa perangkat suara.
- Perpindahan lagu memakai dua trek dengan fade 0,35 detik. Saat jendela tidak
  fokus, loop utama berhenti memperbarui simulasi dan audio dijeda dengan
  `Audio::setPaused`, lalu melanjutkan tanpa melompat karena akumulasi waktu
  tetap diperbarui.
- Ikon jendela di Linux digambar secara prosedural sebagai data ARGB untuk
  `_NET_WM_ICON`; di Windows ikon dimuat dari resource `.ico` yang ditanam ke
  `rpgcpp.exe`. Ikon launcher desktop Linux memakai SVG terpisah agar tetap
  tajam di berbagai ukuran.

## Pemecahan Masalah

- **`Tidak bisa membuka display X11`**: jalankan dari sesi grafis dengan
  `DISPLAY` yang benar, atau gunakan `--headless`.
- **Gagal build karena `X11/Xlib.h` tidak ditemukan**: pasang `libx11-dev`
  atau jalankan `./install.sh` yang akan menawarkan pemasangan dependensi.
- **Windows: `g++` tidak ditemukan atau build gagal pada `windows.h`**: buka
  **MSYS2 MinGW 64-bit**, pasang
  `pacman -S --needed mingw-w64-x86_64-gcc make git`, lalu jalankan ulang
  `./install.sh`.
- **Windows: game tidak muncul saat dijalankan dari terminal**: biner memakai
  subsistem GUI. Pesan `--headless`/`--help` tetap dicetak ke terminal asal
  melalui `AttachConsole`; jika terminal tidak mendukungnya, jalankan lewat
  `cmd /c rpgcpp.exe --help` atau periksa `settings.cfg`.
- **Windows: SmartScreen memperingatkan biner tanpa tanda tangan**: pilih
  **More info > Run anyway**, atau build sendiri dari sumber dengan
  `./install.sh`.
- **`git clone` gagal saat instalasi**: periksa koneksi dan alamat repositori,
  lalu jalankan `./install.sh --repo URL` untuk memakai fork atau cermin lokal.
- **Jendela tidak menerima input**: klik jendela sekali untuk memberi fokus,
  lalu gunakan tombol arah dan `Z`/`Enter`.
- **Tidak ada suara**: di Linux pastikan `libasound.so.2` dan perangkat audio
  tersedia; di Windows pastikan perangkat output aktif dan tidak dipakai mode
  eksklusif. Cek pesan `audio:` saat mulai, lalu buka **Menu > Audio** untuk
  memastikan volume tidak nol dan bisukan tidak aktif. Gunakan `--no-audio` jika
  ingin memastikan mode senyap.
- **Ingin mulai dari awal**: mulai **Mulai Baru** dari judul (konfirmasi muncul
  bila sudah ada simpanan), atau hapus `savegame*.dat` di folder game.
- **Pengaturan tidak berubah**: periksa `settings.cfg` di folder game
  (`~/.aetheria/settings.cfg` pada instalasi) dan pastikan `--settings` tidak
  diarahkan ke berkas lain.
- **Ikon launcher tidak muncul**: pastikan
  `~/.local/share/icons/hicolor/scalable/apps/aetheria.svg` ada, lalu jalankan
  `gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor` atau masuk ulang ke
  sesi desktop.

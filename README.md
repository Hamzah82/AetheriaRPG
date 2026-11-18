# Aetheria: Bayangan Terakhir

RPG 2D berbahasa Indonesia yang ditulis dengan C++17 dan X11. Seluruh grafis, font,
efek, musik, dan suara dibuat secara prosedural; tidak ada berkas aset eksternal
dan tidak ada dependensi SFML/SDL.

Pemain menjelajahi Desa Elmwood, Padang Aetheria, dan Gua Bayangan untuk merebut
kembali Batu Matahari dari Warden Bayangan.

## Fitur

- Tiga peta yang saling terhubung lewat portal, lengkap dengan NPC, peti harta,
  papan petunjuk, dan bangunan.
- Pertarungan turn-based: Serang, Sihir (Bola Api, Petir, Pulihkan), Item, dan Kabur.
- Monster Slime, Kelelawar, dan Serigala dengan kelemahan elemen masing-masing.
- Sistem level, XP, emas, senjata, armor, dan ramuan.
- Toko, penginapan, menu status, serta simpan/muat permainan.
- Musik chiptune dan efek suara prosedural yang mengikuti state permainan, dengan
  halaman **Menu > Audio** untuk mengatur volume musik, volume efek, dan bisukan.
- Boss Warden Bayangan dengan pertarungan khusus, dialog, dan ending.
- Renderer software 320×240 yang diperbesar ke jendela X11 (bawaan 3× menjadi
  960×720), sehingga tampilan tetap tajam dan ringan.
- Mode `--headless` dan `--screenshot` untuk pengujian tanpa jendela.

## Kebutuhan

- Linux dengan sesi X11 (misalnya Linux Mint, Ubuntu, Debian).
- Compiler C++17 (`g++` atau `clang++`).
- `make`.
- Header pengembangan X11 (`libX11`).
- Opsional: `libasound.so.2` untuk musik dan efek suara. Tanpa pustaka atau
  perangkat audio, game tetap berjalan dalam mode senyap.

Di Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential libx11-dev
```

Tidak diperlukan pustaka grafis lain. Header ALSA **tidak** dibutuhkan karena
`libasound.so.2` dimuat saat runtime; cukup paket runtime `libasound2` yang
biasanya sudah terpasang.

## Build dan Menjalankan

```bash
make -j4
./rpgcpp
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

Saat dialog, tombol konfirmasi atau batal akan mempercepat teks lalu melanjutkan
ke halaman berikutnya. Di dalam pertarungan, gunakan tombol arah untuk memilih
perintah dan `X` untuk kembali dari submenu Sihir/Item. Di halaman **Audio**,
tombol kiri/kanan mengubah volume atau pilihan, `Z` memilih, dan `X` kembali.

## Alur Permainan

1. Mulai permainan baru dari judul. Intro narator akan memperkenalkan Lembah
   Aetheria.
2. Temui **Tetua Ardan** di Desa Elmwood untuk menerima quest.
3. Keluar desa melalui jalan di sisi selatan menuju **Padang Aetheria**.
4. Monster muncul secara acak: di rumput tinggi Padang Aetheria dan di lantai
   Gua Bayangan. Peti harta tersebar di beberapa lokasi.
5. Masuk ke **Gua Bayangan** di timur laut padang (melewati jembatan tua).
6. Kalahkan **Warden Bayangan** di ruang paling dalam untuk mendapatkan
   **Batu Matahari**.
7. Kembali ke Tetua Ardan untuk menyelesaikan permainan dan melihat ending.

Desa Elmwood aman dari pertarungan acak. Penginapan memulihkan HP dan MP penuh
dengan biaya 10 emas (hanya jika pemain cukup lelah dan memiliki emas).

## Pertarungan

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

## Musik dan Efek Suara

Seluruh audio disintesis saat program berjalan dengan gelombang kotak, segitiga,
sinus, gergaji, dan noise; tidak ada berkas musik atau suara yang dimuat.

- Tujuh lagu latar: judul, desa, padang, gua, pertarungan, boss, dan ending.
  Lagu berganti otomatis mengikuti state permainan dan peta.
- Efek suara untuk langkah, menu, peti, koin, ramuan, serangan, serangan kritis,
  luka, sihir api/petir, pulih, kabur, menang, kalah, naik level, simpan, portal,
  dan auman boss.
- **Menu > Audio** menyediakan pengaturan volume musik, volume efek suara, dan
  bisukan. Pengaturan berlaku langsung tanpa perlu menyimpan permainan.
- Audio bersifat opsional. Jika `libasound.so.2` atau perangkat suara tidak
  tersedia, game menampilkan pesan dan berjalan tanpa suara.
- Gunakan `--no-audio` untuk mematikan audio sepenuhnya sejak awal.

## Simpan dan Muat

- Menu utama saat menjelajah memiliki pilihan **Simpan**.
- Berkas simpanan bawaan adalah `savegame.dat` di direktori kerja saat ini.
- Pilihan **Lanjutkan** pada layar judul akan memuat berkas tersebut.
- Lokasi berkas dapat diubah dengan `--save PATH`.
- Format simpanan berupa teks dan ditulis secara atomik melalui berkas sementara.

## Opsi Command Line

```text
./rpgcpp [opsi]

  --scale N        perbesaran jendela (1-6, bawaan 3)
  --seed N         benih acak
  --save PATH      lokasi berkas simpanan
  --headless       uji jalan tanpa jendela
  --frames N       jumlah frame uji headless
  --no-audio       jalankan tanpa musik dan efek suara
  --screenshot DIR tulis tangkapan layar lalu keluar
  --help, -h       tampilkan bantuan
```

Contoh:

```bash
./rpgcpp --scale 4
./rpgcpp --no-audio
./rpgcpp --headless --frames 1200
./rpgcpp --screenshot /tmp/opencode/shots
```

Mode `--screenshot` memakai berkas simpanan terpisah (`PATH.shot`) agar tidak
mengganggu simpanan permainan biasa. Hasilnya adalah PNG 960×720 dari berbagai
kondisi permainan.

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
src/game_render.cpp tampilan judul, menu, toko, status, ending
src/platform.h/.cpp jendela X11, input, presentasi piksel
src/main.cpp        CLI, loop utama, headless, screenshot
tests/test_main.cpp rangkaian tes regresi
Makefile            target build, test, run, shots, clean
```

## Pengujian

```bash
make test
```

Tes mencakup RNG dan matematika dasar, statistik pemain, leveling, konektivitas
peta (BFS), collision, portal, peti, dialog, toko, penginapan, seluruh alur
pertarungan, simpan/muat (termasuk berkas rusak), menu, game over, ending,
determinisme render, ekspor PNG, pemetaan warna X11, render setiap state
permainan, serta sintesis audio (durasi, puncak, loop mulus, determinisme) dan
pengaturan volume/bisukan tanpa perangkat suara. Tes tidak pernah membuka
perangkat audio sehingga tetap senyap dan deterministik.

Uji asap tanpa jendela:

```bash
./rpgcpp --headless --frames 1200
```

Keluaran yang diharapkan berakhir dengan baris seperti:

```text
headless ok: frames=1200 state=1 map=0 hp=42 checksum=...
```

## Catatan Teknis

- Resolusi internal tetap 320×240; presentasi X11 memakai penskalaan
  nearest-neighbour agar piksel tetap tegas.
- Warna piksel dipetakan ke mask merah/hijau/biru milik visual X11, sehingga
  tampilan benar pada layout RGB maupun BGR (termasuk kanal 10-bit).
- Logika permainan tidak bergantung pada X11, sehingga dapat diuji secara
  headless dan deterministik dengan benih acak tetap.
- Font 5×7 dan seluruh sprite digambar langsung ke framebuffer, tanpa memuat
  berkas gambar.
- Mesin audio memakai satu thread pencampuran dengan potongan 512 frame pada
  44100 Hz mono S16. `libasound.so.2` dimuat melalui `dlopen`, sehingga build
  tidak memerlukan header ALSA dan game tetap bisa dijalankan tanpa audio.
- Musik dan efek suara dibangkitkan secara deterministik dari not dan parameter
  tetap; hasilnya sama pada setiap mesin dan dapat diuji tanpa perangkat suara.

## Pemecahan Masalah

- **`Tidak bisa membuka display X11`**: jalankan dari sesi grafis dengan
  `DISPLAY` yang benar, atau gunakan `--headless`.
- **Gagal build karena `X11/Xlib.h` tidak ditemukan**: pasang `libx11-dev`.
- **Jendela tidak menerima input**: klik jendela sekali untuk memberi fokus,
  lalu gunakan tombol arah dan `Z`/`Enter`.
- **Tidak ada suara**: pastikan `libasound.so.2` dan perangkat audio tersedia,
  cek pesan `audio:` saat mulai, lalu buka **Menu > Audio** untuk memastikan
  volume tidak nol dan bisukan tidak aktif. Gunakan `--no-audio` jika ingin
  memastikan mode senyap.
- **Ingin mulai dari awal**: hapus `savegame.dat` atau jalankan dengan
  `--save` ke lokasi berkas baru.

# Aetheria: Bayangan Terakhir

RPG 2D berbahasa Indonesia yang ditulis dengan C++17 dan X11. Seluruh grafis, font,
dan efek dibuat secara prosedural; tidak ada berkas aset eksternal dan tidak ada
dependensi SFML/SDL.

Pemain menjelajahi Desa Elmwood, Padang Aetheria, dan Gua Bayangan untuk merebut
kembali Batu Matahari dari Warden Bayangan.

## Fitur

- Tiga peta yang saling terhubung lewat portal, lengkap dengan NPC, peti harta,
  papan petunjuk, dan bangunan.
- Pertarungan turn-based: Serang, Sihir (Bola Api, Petir, Pulihkan), Item, dan Kabur.
- Monster Slime, Kelelawar, dan Serigala dengan kelemahan elemen masing-masing.
- Sistem level, XP, emas, senjata, armor, dan ramuan.
- Toko, penginapan, menu status, serta simpan/muat permainan.
- Boss Warden Bayangan dengan pertarungan khusus, dialog, dan ending.
- Renderer software 320×240 yang diperbesar ke jendela X11 (bawaan 3× menjadi
  960×720), sehingga tampilan tetap tajam dan ringan.
- Mode `--headless` dan `--screenshot` untuk pengujian tanpa jendela.

## Kebutuhan

- Linux dengan sesi X11 (misalnya Linux Mint, Ubuntu, Debian).
- Compiler C++17 (`g++` atau `clang++`).
- `make`.
- Header pengembangan X11 (`libX11`).

Di Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential libx11-dev
```

Tidak diperlukan pustaka grafis lain.

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
perintah dan `X` untuk kembali dari submenu Sihir/Item.

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
  --screenshot DIR tulis tangkapan layar lalu keluar
  --help, -h       tampilkan bantuan
```

Contoh:

```bash
./rpgcpp --scale 4
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
determinisme render, ekspor PNG, pemetaan warna X11, dan render setiap state
permainan.

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

## Pemecahan Masalah

- **`Tidak bisa membuka display X11`**: jalankan dari sesi grafis dengan
  `DISPLAY` yang benar, atau gunakan `--headless`.
- **Gagal build karena `X11/Xlib.h` tidak ditemukan**: pasang `libx11-dev`.
- **Jendela tidak menerima input**: klik jendela sekali untuk memberi fokus,
  lalu gunakan tombol arah dan `Z`/`Enter`.
- **Ingin mulai dari awal**: hapus `savegame.dat` atau jalankan dengan
  `--save` ke lokasi berkas baru.

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <vector>
#include <iterator>

#include <algorithm>

#include "rom.hpp"

const boost::array<char, 4> rom::NORMAL_HEADER = { { 'N', 'E', 'S', 0x01a } };

rom::rom(std::ifstream& file)
{
	// When you open a file in binary mode IT SKIPS WHITESPACE BY DEFAULT
	// WTF C++? THERE IS NO CONCEPT OF 'WHITESPACE' IN A BINARY FILE. WTF?
	file >> std::noskipws;
	
	file.seekg(0, std::ios::end);
	std::streampos size = file.tellg();

    file.seekg(0, std::ios::beg);
	std::copy(std::istream_iterator<char>(file), std::istream_iterator<char>(),
			  std::back_inserter(raw_nes_data_));
    file.close();

	// Read ROM according to docs at:
	// http://www.sadistech.com/nesromtool/romdoc.html

	// array comparer only checks as many elements as are in the array
	if(!std::equal(NORMAL_HEADER.begin(), NORMAL_HEADER.begin() + 4, raw_nes_data_.begin()))
	{
		throw std::runtime_error("ROM Header check failed.");
	}

	std::cout << "Loaded rom Size: " << raw_nes_data_.size() << std::endl;

	std::cout << to_string();
}

std::vector<sprite*> rom::construct_sprites()
{
	std::vector<sprite*> sprites;

	for(int bank = 0; bank < num_chr_banks(); ++bank) {
		for(int sprite_offset = 0; sprite_offset < CHR_BANK_SIZE; sprite_offset += 16)
			sprites.push_back(
				new sprite(
					&raw_nes_data_[chr_bank_offset(bank) + sprite_offset]
					));
	}

	return sprites;
}

int rom::num_prg_banks() const
{
	// 4 is constant offset determined by .NES format
	return raw_nes_data_[4];
}

int rom::num_chr_banks() const
{
	// 5 is constant offset determined by .NES format
	return raw_nes_data_[5];
}

boost::shared_ptr<mirror_mode> rom::stored_mirror_mode() const
{
	// This bit overrides horizontal/vertical choice
	if(raw_nes_data_[6] & 8)
		return boost::shared_ptr<mirror_mode>(new four_screen_mirror_mode);
	
	if(raw_nes_data_[6] & 1)
		return boost::shared_ptr<mirror_mode>(new horizontal_mirror_mode);
	else
		return boost::shared_ptr<mirror_mode>(new vertical_mirror_mode);
}

bool rom::battery_backed_ram() const
{
	return raw_nes_data_[6] & 2;
}

bool rom::trainer() const
{
	return raw_nes_data_[6] & 4;
}

uint8_t rom::mapper_number() const
{
	uint8_t lower_bits = 0;
	uint8_t higher_bits = 0;

	// Lower four bits in upper four bits of byte 6
	lower_bits |= raw_nes_data_[6] & ((1 << 4) |
									  (1 << 5) |
									  (1 << 6) |
									  (1 << 7));

	// Upper four bits in upper four bits of byte 7
	higher_bits |= raw_nes_data_[7] & ((1 << 4) |
									   (1 << 5) |
									   (1 << 6) |
									   (1 << 7));

	uint8_t number = 0;
	number |= lower_bits;
	number |= (higher_bits << 4);

	return number;
}

uint8_t rom::num_ram_banks() const
{
	if(raw_nes_data_[8] == 0)
		return 1;

	return raw_nes_data_[8];
}

int rom::prg_bank_offset(int i) const
{
	return BANK_START_OFFSET +
		(trainer() ? 512 : 0) + // If a trainer is present takes 512 after header
		PRG_BANK_SIZE * i;
}

int rom::chr_bank_offset(int i) const
{
	return prg_bank_offset(num_prg_banks()) + CHR_BANK_SIZE * i;
}

std::vector<char*> rom::prg_banks()
{
	std::vector<char*> result;
	
	for(int i = 0; i < num_prg_banks(); ++i)
		result.push_back(&raw_nes_data_[prg_bank_offset(i)]);

	return result;
}

std::vector<char*> rom::chr_banks()
{
	std::vector<char*> result;

	for(int i = 0; i < num_chr_banks(); ++i)
		result.push_back(&raw_nes_data_[chr_bank_offset(i)]);
	
	return result;
}

std::string rom::title() const
{
	size_t title_offset = chr_bank_offset(num_chr_banks());
	const char* title_start = &raw_nes_data_[title_offset];

	if(*title_start == 0xFF)
	    ++title_start; // Some roms the title starts 127 bytes back, not 128

	// Not every ROM comes with title information
	if(title_start > &raw_nes_data_.back())
		return std::string(" ");

	return std::string(title_start);
}

std::string rom::to_string() const
{
	std::ostringstream o;

	o << "PRG Banks: " << num_prg_banks() << std::endl
	  << "CHR Banks: " << num_chr_banks() << std::endl
	  << "Title: " << title() << std::endl
	  << "Mirror Mode: " << *stored_mirror_mode() << std::endl
	  << "Battery Backed RAM: " << (battery_backed_ram() ? "Yes" : "No") << std::endl
	  << "Trainer Provided: " << (trainer() ? "Yes" : "No") << std::endl
	  << "Mapper Number: " << int(mapper_number()) << std::endl
	  << "RAM banks: " << int(num_ram_banks()) << std::endl;

	return o.str();
}

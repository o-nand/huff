#include <algorithm>
#include <bitset>
#include <cmath>
#include <format>
#include <stack>

#include "encoder.hpp"

std::unordered_map<char, uint32_t> extract_frequencies(std::ifstream& input) {
    std::unordered_map<char, uint32_t> frequencies(SUPPORTED_CHARACTERS);
    std::string line;

    while(std::getline(input, line)) {
        for(const char& character : line + NEW_LINE) {
            if(character < FIRST_CHARACTER)
                throw std::invalid_argument(std::format("unsupported character: {}\n", character));
            frequencies[character]++;
        }
    }

    frequencies[END_OF_TEXT]++;

    return frequencies;
}

Node build_huffman_tree(const std::unordered_map<char, uint32_t>& frequencies) {
    min_priority_queue<Node> nodes;

    for(const auto& [symbol, frequency] : frequencies)
        nodes.push(Node{symbol, frequency});

    while(nodes.size() > 1) {
        const Node first = nodes.top();
        nodes.pop();
        const Node second = nodes.top();
        nodes.pop();

        Node parent;
        parent.frequency = first.frequency + second.frequency;
        parent.left = new Node{first};
        parent.right = new Node{second};

        nodes.push(parent);
    }

    return nodes.top();
}

std::vector<std::pair<char, std::string>> generate_huffman_codes(const Node& root) {
    std::vector<std::pair<char, std::string>> huffman_codes;

    std::stack<std::pair<Node, std::string>> nodes;
    nodes.push(std::make_pair(root, std::string{}));

    while(!nodes.empty()) {
        const Node current = nodes.top().first;
        std::string code = nodes.top().second;
        nodes.pop();

        if(current.left)
            nodes.push(std::make_pair(*current.left, code + '0'));

        if(current.right)
            nodes.push(std::make_pair(*current.right, code + '1'));

        if(current.symbol != '\0')
            huffman_codes.emplace_back(current.symbol, code);
    }

    std::sort(huffman_codes.begin(), huffman_codes.end(), [](const auto& left, const auto& right) {
        const char left_symbol = left.first;
        const char right_symbol = right.first;
        const size_t left_size = left.second.size();
        const size_t right_size = right.second.size();
        return (left_size == right_size) ? (left_symbol < right_symbol) : (left_size < right_size);
    });

    return huffman_codes;
}

std::string next_binary(std::string number) {
    size_t carry = 0;

    auto flip = [](const char& bit) -> char {
        return bit == '0' ? '1' : '0';
    };

    for(size_t index = 1; index <= (1 + carry); index++) {
        if(carry && index > number.size()) {
            number = '1' + number;
            carry--;
            continue;
        }
        char& last = number[number.size() - index];
        if(last == '1') carry++;
        last = flip(last);
    }

    return number;
}

std::unordered_map<char, std::string> generate_canonical_codes(const std::vector<std::pair<char, std::string>>& huffman_codes) {
    std::unordered_map<char, std::string> canonical_codes;

    const std::pair<char, std::string>& front_element = huffman_codes.front();
    for(size_t index = 0; index < front_element.second.size(); index++)
        canonical_codes[front_element.first] += '0';

    std::string last_code = canonical_codes[front_element.first];
    for(size_t index = 1; index < huffman_codes.size(); index++) {
        const std::pair<char, std::string>& current_element = huffman_codes.at(index);

        std::string current_code = next_binary(last_code);
        while(current_code.size() < huffman_codes.at(index).second.size()) current_code += '0';

        last_code = current_code;
        canonical_codes[current_element.first] = current_code;
    }

    return canonical_codes;
}

std::vector<char> encode_codes_length(std::unordered_map<char, std::string>& code_table) {
    std::vector<char> output;
    std::string buffer;

    uint32_t largest_code_length = 0;
    for(const auto& [symbol, code] : code_table) {
        if(code.size() < largest_code_length) continue;
        largest_code_length = code.size();
    }
    const uint32_t bit_count = std::log2(largest_code_length) + 1;

    output.push_back(bit_count);

    for(char character = FIRST_CHARACTER; character < SUPPORTED_CHARACTERS; character++) {
        const uint32_t code_length = code_table[character].size();
        std::string binary = std::bitset<MAX_BITS>(code_length).to_string().substr(MAX_BITS - bit_count);

        for(const char& bit : binary) {
            buffer += bit;
            if(buffer.size() % 8 != 0) continue;
            output.push_back(std::stoi(buffer, nullptr, 2));
            buffer.clear();
        }
    }

    if(buffer.size() % 8 != 0) {
        while(buffer.size() % 8 != 0) buffer += '0';
        output.push_back(std::stoi(buffer, nullptr, 2));
    }

    return output;
}

std::vector<char> encode_content(std::ifstream& file, std::unordered_map<char, std::string>& code_table) {
    std::vector<char> output;

    std::string buffer;
    std::string line;

    while(std::getline(file, line)) {
        for(const char& character : line + NEW_LINE) {
            const std::string code = code_table[character];
            for(size_t index = 0; index < code.size(); index++) {
                buffer += code.at(index);
                if(buffer.size() % 8 != 0) continue;
                output.push_back(std::stoi(buffer, nullptr, 2));
                buffer.clear();
            }
        }
    }

    for(size_t index = 0; index < code_table[END_OF_TEXT].size(); index++) {
        buffer += code_table[END_OF_TEXT].at(index);
        if(buffer.size() % 8 != 0) continue;
        output.push_back(std::stoi(buffer, nullptr, 2));
        buffer.clear();
    }

    if(buffer.size() % 8 != 0) {
        while(buffer.size() % 8 != 0) buffer += '0';
        output.push_back(std::stoi(buffer, nullptr, 2));
    }

    return output;
}

void create_compressed_file(const std::string& file_path) {
    std::ofstream output_file(std::format("{}.hf", file_path), std::ios::binary | std::ios::out);
    std::ifstream input_file(file_path);

    const std::unordered_map<char, uint32_t> frequencies = extract_frequencies(input_file);
    const Node tree = build_huffman_tree(frequencies);

    const std::vector<std::pair<char, std::string>> huffman_codes = generate_huffman_codes(tree);
    std::unordered_map<char, std::string> canonical_codes = generate_canonical_codes(huffman_codes);

    input_file.clear();
    input_file.seekg(0, input_file.beg);

    const std::vector<char> encoded_codes = encode_codes_length(canonical_codes);
    const std::vector<char> encoded_content = encode_content(input_file, canonical_codes);

    for(const char& bit : encoded_codes) output_file.put(bit);
    for(const char& bit : encoded_content) output_file.put(bit);

    input_file.close();
    output_file.close();
}
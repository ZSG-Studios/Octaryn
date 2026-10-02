#include "../../../octaryn-client/Source/Ui/DeclaredScreen/DeclaredBitmapText.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <iostream>
#include <iterator>

struct Case {std::string text;double width{};};
struct Input {octaryn::client::ui::BitmapFont font;double scale=1;std::vector<Case> cases;};
struct Output {std::string text,markup;double width{},height{};};
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);if(!stream)return 2;
  std::string bytes((std::istreambuf_iterator<char>(stream)),{});
  if(bytes.size()>1024*1024)return 2;
  Input input;if(glz::read_json(input,bytes) || input.cases.size()>2048)return 2;
  std::vector<Output> output;
  for(const auto& sample:input.cases) {
    octaryn::client::ui::DocumentBinding binding;binding.kind="bitmap_text";
    binding.scale=input.scale;binding.wrap_width=sample.width;binding.fit_text_height=true;
    Output row{sample.text,{},sample.width};
    if(!octaryn::client::ui::bitmap_markup(input.font,binding,sample.text,sample.width,row.markup,
        octaryn::client::ui::bitmap_white,&row.height))return 3;
    output.push_back(std::move(row));
  }
  std::cout<<glz::write_json(output).value()<<'\n';
}

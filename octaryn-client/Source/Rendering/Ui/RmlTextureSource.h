#pragma once
#include <string>
#include <string_view>

namespace octaryn::client::rendering {
struct RmlTextureSource {
  std::string_view filename;
  bool linear{}, wrap{}, straight{};
};
inline RmlTextureSource rml_texture_source(std::string_view source) {
  constexpr std::string_view linear_wrap = "octaryn-ui-linear-wrap:";
  constexpr std::string_view linear = "octaryn-ui-linear:";
  constexpr std::string_view wrap = "octaryn-ui-wrap:";
  constexpr std::string_view straight_linear_wrap = "octaryn-ui-straight-linear-wrap:";
  constexpr std::string_view straight_linear = "octaryn-ui-straight-linear:";
  constexpr std::string_view straight_wrap = "octaryn-ui-straight-wrap:";
  constexpr std::string_view straight = "octaryn-ui-straight:";
  if (source.starts_with(straight_linear_wrap)) return {source.substr(straight_linear_wrap.size()), true, true, true};
  if (source.starts_with(straight_linear)) return {source.substr(straight_linear.size()), true, false, true};
  if (source.starts_with(straight_wrap)) return {source.substr(straight_wrap.size()), false, true, true};
  if (source.starts_with(straight)) return {source.substr(straight.size()), false, false, true};
  if (source.starts_with(linear_wrap)) return {source.substr(linear_wrap.size()), true, true};
  if (source.starts_with(linear)) return {source.substr(linear.size()), true, false};
  if (source.starts_with(wrap)) return {source.substr(wrap.size()), false, true};
  return {source, false, false};
}
inline std::string rml_wrapped_texture_source(std::string_view source) {
  const auto policy = rml_texture_source(source);
  return std::string(policy.straight ? (policy.linear ? "octaryn-ui-straight-linear-wrap:" : "octaryn-ui-straight-wrap:") :
                    (policy.linear ? "octaryn-ui-linear-wrap:" : "octaryn-ui-wrap:")) +
         std::string(policy.filename);
}
}

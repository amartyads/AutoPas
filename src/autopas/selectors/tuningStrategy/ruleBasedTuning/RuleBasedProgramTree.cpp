#include "RuleBasedProgramTree.h"

namespace autopas::rule_syntax {
void Variable::generateCode(CodeGenerationContext &context, RuleVM::Program &program) const {
  program.instructions.emplace_back(RuleVM::LOADA, context.addressOf(definition->variable));
  context.allocateStack(1);
}

Type Variable::getType() const { return definition->value->getType(); }

void UnaryOperator::generateCode(CodeGenerationContext &context, RuleVM::Program &program) const {
  child->generateCode(context, program);

  program.instructions.emplace_back(RuleVM::NOT);
}

void BinaryOperator::generateCode(CodeGenerationContext &context, RuleVM::Program &program) const {
  left->generateCode(context, program);
  right->generateCode(context, program);

  static const std::map<Operator, RuleVM::CMD> opMap{
      {LESS, RuleVM::LESS}, {GREATER, RuleVM::GREATER}, {AND, RuleVM::AND}, {OR, RuleVM::OR},
      {ADD, RuleVM::ADD},   {SUB, RuleVM::SUB},         {MUL, RuleVM::MUL}, {DIV, RuleVM::DIV}};
  auto opCode = opMap.at(op);
  program.instructions.emplace_back(opCode);

  context.freeStack(1);
}

void ConfigurationOrder::generateCode(CodeGenerationContext &context, RuleVM::Program &program) const {
  auto idx = context.addConfigurationOrder(*this);
  program.instructions.emplace_back(RuleVM::OUTPUTC, idx);
}

void CodeGenerationContext::addLocalVariable(const Define &definition) {
  addressEnvironment.insert({definition.variable, {&definition, addressEnvironment.size()}});
}

void CodeGenerationContext::addGlobalVariable(const Define &definition) {
  initialNumVariables++;
  addLocalVariable(definition);
}

[[nodiscard]] size_t CodeGenerationContext::addressOf(const std::string &name) const {
  return addressEnvironment.at(name).second;
}

const Define *CodeGenerationContext::definitionOf(const std::string &name) const {
  return addressEnvironment.at(name).first;
}

}  // namespace autopas::rule_syntax